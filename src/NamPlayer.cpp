#include "NamPlayer.hpp"
#include <osdialog.h>
#include <algorithm>

NamPlayer::NamPlayer() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    
    // Input/Output gain
    configParam(INPUT_PARAM, 0.f, 2.f, 1.f, "Input Level", "", 0.f, 100.f);
    configParam(OUTPUT_PARAM, 0.f, 2.f, 1.f, "Output Level", "", 0.f, 100.f);
    
    // Noise Gate parameters
    configParam(GATE_THRESHOLD_PARAM, -80.f, 0.f, -60.f, "Gate Threshold", " dB");
    configParam(GATE_ATTACK_PARAM, 0.1f, 50.f, 0.5f, "Gate Attack", " ms");
    configParam(GATE_RELEASE_PARAM, 10.f, 500.f, 100.f, "Gate Release", " ms");
    configParam(GATE_HOLD_PARAM, 10.f, 500.f, 50.f, "Gate Hold", " ms");
    
    // Tone Stack (5-band EQ) - range 0-1, centered at 0.5 for 0dB
    configParam(BASS_PARAM, 0.f, 1.f, 0.5f, "Bass", " dB", 0.f, 24.f, -12.f);
    configParam(MIDDLE_PARAM, 0.f, 1.f, 0.5f, "Middle", " dB", 0.f, 24.f, -12.f);
    configParam(TREBLE_PARAM, 0.f, 1.f, 0.5f, "Treble", " dB", 0.f, 24.f, -12.f);
    configParam(PRESENCE_PARAM, 0.f, 1.f, 0.5f, "Presence", " dB", 0.f, 24.f, -12.f);
    configParam(DEPTH_PARAM, 0.f, 1.f, 0.5f, "Depth", " dB", 0.f, 24.f, -12.f);
    
    configInput(AUDIO_INPUT, "Audio");
    configOutput(AUDIO_OUTPUT, "Audio");
    
    // Initialize DSP wrapper
    namDsp = std::make_unique<NamDSP>();
    
    // Pre-allocate buffers
    inputBuffer.resize(BLOCK_SIZE, 0.f);
    outputBuffer.resize(BLOCK_SIZE, 0.f);
    displayBuffer.resize(DISPLAY_BUFFER_SIZE, 0.f);
    
    // Enable fast tanh for better performance
    nam::activations::Activation::enable_fast_tanh();
}

NamPlayer::~NamPlayer() {
    // Wait for any loading thread to finish
    if (loadThread.joinable()) {
        loadThread.join();
    }
}

void NamPlayer::process(const ProcessArgs& args) {
    if (hasPendingDsp.exchange(false, std::memory_order_acq_rel)) {
        namDsp = std::move(pendingDsp);
    }
    if (hasPendingUnload.exchange(false, std::memory_order_acq_rel)) {
        if (namDsp) {
            namDsp->unloadModel();
        }
    }
    if (hasPendingSampleRate.exchange(false, std::memory_order_acq_rel)) {
        double newRate = pendingSampleRate.load(std::memory_order_acquire);
        if (namDsp) {
            namDsp->setSampleRate(newRate);
        }
        if (pendingDsp) {
            pendingDsp->setSampleRate(newRate);
        }
    }
    // Get input with gain (line-level normalization: ±5V -> ±1.0)
    float inputGain = params[INPUT_PARAM].getValue();
    float input = inputs[AUDIO_INPUT].getVoltage() / 5.f * inputGain;
    
    // Update noise gate parameters from knobs
    if (namDsp) {
        float threshold = params[GATE_THRESHOLD_PARAM].getValue();
        float attack = params[GATE_ATTACK_PARAM].getValue();
        float release = params[GATE_RELEASE_PARAM].getValue();
        float hold = params[GATE_HOLD_PARAM].getValue();
        namDsp->setNoiseGate(threshold, attack, release, hold);
    }
    
    // Update tone stack parameters from knobs (convert 0-1 to -12 to +12 dB)
    if (namDsp) {
        float bass = (params[BASS_PARAM].getValue() - 0.5f) * 24.f;
        float middle = (params[MIDDLE_PARAM].getValue() - 0.5f) * 24.f;
        float treble = (params[TREBLE_PARAM].getValue() - 0.5f) * 24.f;
        float presence = (params[PRESENCE_PARAM].getValue() - 0.5f) * 24.f;
        float depth = (params[DEPTH_PARAM].getValue() - 0.5f) * 24.f;
        namDsp->setToneStack(bass, middle, treble, presence, depth);
    }
    
    // Check if model is loaded - passthrough if not
    if (!namDsp || !namDsp->isModelLoaded() || isLoading) {
        // Passthrough: output = input (with gain applied)
        float outputGain = params[OUTPUT_PARAM].getValue();
        outputs[AUDIO_OUTPUT].setVoltage(input * outputGain * 5.f);
        
        lights[MODEL_LIGHT].setBrightness(isLoading ? 0.5f : 0.f);
        lights[SAMPLE_RATE_LIGHT].setBrightness(0.f);
        lights[GATE_LIGHT].setBrightness(0.f);
        return;
    }
    
    // Model loaded indicator
    lights[MODEL_LIGHT].setBrightness(1.f);
    
    // Sample rate mismatch indicator
    lights[SAMPLE_RATE_LIGHT].setBrightness(namDsp->isSampleRateMismatched() ? 1.f : 0.f);
    
    // Gate activity indicator
    lights[GATE_LIGHT].setBrightness(namDsp->isGateOpen() ? 1.f : 0.f);
    
    // Accumulate input
    inputBuffer[bufferPos] = input;
    
    // Get output from previous block
    float output = outputBuffer[bufferPos];
    
    // Advance position
    bufferPos++;
    
    // Process when buffer is full
    if (bufferPos >= BLOCK_SIZE) {
        if (namDsp && namDsp->isModelLoaded()) {
            namDsp->process(inputBuffer.data(), outputBuffer.data(), BLOCK_SIZE);
        }
        bufferPos = 0;
    }
    
    // Apply output gain and send (scale back to ±5V)
    float outputGain = params[OUTPUT_PARAM].getValue();
    float finalOutput = output * outputGain;
    outputs[AUDIO_OUTPUT].setVoltage(finalOutput * 5.f);
    
    // Update display buffer with output sample (normalized to ±1)
    displayBuffer[displayBufferPos] = finalOutput;
    displayBufferPos = (displayBufferPos + 1) % DISPLAY_BUFFER_SIZE;
}

void NamPlayer::onSampleRateChange(const SampleRateChangeEvent& e) {
    currentSampleRate.store(e.sampleRate, std::memory_order_release);
    pendingSampleRate.store(e.sampleRate, std::memory_order_release);
    hasPendingSampleRate.store(true, std::memory_order_release);
}

void NamPlayer::loadModel(const std::string& path) {
    // Don't start new load if already loading
    if (isLoading) {
        return;
    }
    
    // Wait for previous load to finish
    if (loadThread.joinable()) {
        loadThread.join();
    }
    
    isLoading = true;
    loadSuccess = false;
    
    loadThread = std::thread([this, path]() {
        try {
            auto newDsp = std::make_unique<NamDSP>();
            
            if (newDsp->loadModel(path)) {
                // Initialize with current sample rate
                newDsp->setSampleRate(currentSampleRate.load(std::memory_order_acquire));
                
                // Swap DSP
                pendingDsp = std::move(newDsp);
                hasPendingDsp.store(true, std::memory_order_release);
                
                loadSuccess = true;
            }
        } catch (const std::exception& e) {
            WARN("Failed to load NAM model: %s", e.what());
            loadSuccess = false;
        }
        
        isLoading = false;
    });
}

void NamPlayer::unloadModel() {
    hasPendingUnload.store(true, std::memory_order_release);
}

std::string NamPlayer::getModelPath() const {
    if (namDsp) {
        return namDsp->getModelPath();
    }
    return "";
}

std::string NamPlayer::getModelName() const {
    if (namDsp) {
        return namDsp->getModelName();
    }
    return "";
}

bool NamPlayer::isSampleRateMismatched() const {
    if (namDsp) {
        return namDsp->isSampleRateMismatched();
    }
    return false;
}

void NamPlayer::getWaveformColorRGB(int& r, int& g, int& b) const {
    switch (waveformColor) {
        case WaveformColor::Green:
            r = 100; g = 255; b = 100;
            break;
        case WaveformColor::BabyBlue:
            r = 100; g = 200; b = 255;
            break;
        case WaveformColor::Amber:
            r = 255; g = 180; b = 50;
            break;
        case WaveformColor::Red:
            r = 255; g = 80; b = 80;
            break;
        case WaveformColor::Purple:
            r = 200; g = 100; b = 255;
            break;
        case WaveformColor::White:
            r = 255; g = 255; b = 255;
            break;
        default:
            r = 100; g = 255; b = 100; // Default to Green
            break;
    }
}

json_t* NamPlayer::dataToJson() {
    json_t* rootJ = json_object();
    std::string path = getModelPath();
    json_object_set_new(rootJ, "modelPath", json_string(path.c_str()));
    json_object_set_new(rootJ, "waveformColor", json_integer(static_cast<int>(waveformColor)));
    return rootJ;
}

void NamPlayer::dataFromJson(json_t* rootJ) {
    json_t* pathJ = json_object_get(rootJ, "modelPath");
    if (pathJ) {
        std::string path = json_string_value(pathJ);
        if (!path.empty()) {
            loadModel(path);
        }
    }
    
    json_t* colorJ = json_object_get(rootJ, "waveformColor");
    if (colorJ) {
        int colorInt = json_integer_value(colorJ);
        if (colorInt >= 0 && colorInt < static_cast<int>(WaveformColor::NUM_COLORS)) {
            waveformColor = static_cast<WaveformColor>(colorInt);
        }
    }
}

// Widget implementation
NamPlayerWidget::NamPlayerWidget(NamPlayer* module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/SWV_21HP_PANEL.svg")));

    // Module is 21HP = 106.68mm wide
    
    // Screws
    addChild(createWidget<ScrewSilver>(Vec(0, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // Input/Output gain knobs (large, top section)
    addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15, 30)), module, NamPlayer::INPUT_PARAM));
    addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(91, 30)), module, NamPlayer::OUTPUT_PARAM));

    // Noise Gate knobs (small, second row)
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(15, 50)), module, NamPlayer::GATE_THRESHOLD_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(34, 50)), module, NamPlayer::GATE_ATTACK_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(53, 50)), module, NamPlayer::GATE_RELEASE_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(72, 50)), module, NamPlayer::GATE_HOLD_PARAM));

    // Tone Stack knobs (small, third row - 5 knobs)
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(15, 70)), module, NamPlayer::BASS_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(34, 70)), module, NamPlayer::MIDDLE_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(53, 70)), module, NamPlayer::TREBLE_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(72, 70)), module, NamPlayer::PRESENCE_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(91, 70)), module, NamPlayer::DEPTH_PARAM));

    // Mono inputs/outputs (bottom)
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15, 100)), module, NamPlayer::AUDIO_INPUT));
    addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(91, 100)), module, NamPlayer::AUDIO_OUTPUT));

    // Lights (top center area)
    addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(45, 20)), module, NamPlayer::MODEL_LIGHT));
    addChild(createLightCentered<SmallLight<YellowLight>>(mm2px(Vec(53, 20)), module, NamPlayer::SAMPLE_RATE_LIGHT));
    addChild(createLightCentered<SmallLight<GreenLight>>(mm2px(Vec(61, 20)), module, NamPlayer::GATE_LIGHT));
    
    // Output waveform display (below tone stack, above I/O)
    OutputDisplay* display = new OutputDisplay();
    display->module = module;
    display->box.pos = mm2px(Vec(2, 82));
    display->box.size = mm2px(Vec(102.68, 12));
    addChild(display);
}

void NamPlayerWidget::appendContextMenu(Menu* menu) {
    NamPlayer* module = dynamic_cast<NamPlayer*>(this->module);
    if (!module) return;

    menu->addChild(new MenuSeparator());
    
    // Submenu for bundled models (from res/models/)
    menu->addChild(createSubmenuItem("Bundled Models", "", [=](Menu* submenu) {
        std::string modelsDir = asset::plugin(pluginInstance, "res/models");
        std::vector<std::string> modelFiles = system::getEntries(modelsDir);
        
        // Sort alphabetically
        std::sort(modelFiles.begin(), modelFiles.end());
        
        for (const std::string& file : modelFiles) {
            if (system::getExtension(file) == ".nam") {
                std::string name = system::getStem(file);
                submenu->addChild(createMenuItem(name, "", [=]() {
                    module->loadModel(file);
                }));
            }
        }
        
        if (modelFiles.empty()) {
            submenu->addChild(createMenuLabel("No models found"));
        }
    }));
    
    // File picker for custom models
    menu->addChild(createMenuItem("Load Custom Model...", "", [=]() {
        char* path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, 
            osdialog_filters_parse("NAM Models:nam"));
        if (path) {
            module->loadModel(path);
            free(path);
        }
    }));

    menu->addChild(createMenuItem("Unload Model", "", [=]() {
        module->unloadModel();
    }, !module->namDsp || !module->namDsp->isModelLoaded()));

    std::string modelName = module->getModelName();
    if (!modelName.empty()) {
        menu->addChild(new MenuSeparator());
        menu->addChild(createMenuLabel("Model: " + modelName));
        if (module->isSampleRateMismatched()) {
            menu->addChild(createMenuLabel("⚠ Sample rate mismatch (resampling active)"));
        }
    }
    
    // Waveform color selection submenu
    menu->addChild(new MenuSeparator());
    menu->addChild(createSubmenuItem("Waveform Color", "", [=](Menu* submenu) {
        const char* colorNames[] = {"Green", "Baby Blue", "Amber", "Red", "Purple", "White"};
        
        for (int i = 0; i < static_cast<int>(NamPlayer::WaveformColor::NUM_COLORS); i++) {
            auto color = static_cast<NamPlayer::WaveformColor>(i);
            submenu->addChild(createMenuItem(colorNames[i], 
                module->waveformColor == color ? "✓" : "",
                [=]() {
                    module->waveformColor = color;
                }
            ));
        }
    }));
}

// OutputDisplay implementation
void OutputDisplay::draw(const DrawArgs& args) {
    drawBackground(args);
    drawWaveform(args);
}

void OutputDisplay::drawBackground(const DrawArgs& args) {
    // Dark background
    nvgBeginPath(args.vg);
    nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
    nvgFillColor(args.vg, nvgRGB(15, 15, 20));
    nvgFill(args.vg);
    
    // Subtle border
    nvgStrokeColor(args.vg, nvgRGB(50, 50, 60));
    nvgStrokeWidth(args.vg, 1.0f);
    nvgStroke(args.vg);
    
    // Center line
    float centerY = box.size.y * 0.5f;
    nvgBeginPath(args.vg);
    nvgMoveTo(args.vg, 0, centerY);
    nvgLineTo(args.vg, box.size.x, centerY);
    nvgStrokeColor(args.vg, nvgRGBA(60, 60, 70, 100));
    nvgStrokeWidth(args.vg, 0.5f);
    nvgStroke(args.vg);
}

void OutputDisplay::drawWaveform(const DrawArgs& args) {
    if (!module)
        return;
    
    float centerY = box.size.y * 0.5f;
    float maxBarHeight = box.size.y * 0.45f;
    
    // Bar settings
    const float barSpacing = kBarWidth + kBarGap;
    int numBars = static_cast<int>(box.size.x / barSpacing);
    if (numBars <= 0) return;
    
    // Calculate samples per bar
    int samplesPerBar = NamPlayer::DISPLAY_BUFFER_SIZE / numBars;
    if (samplesPerBar < 1) samplesPerBar = 1;
    
    // Get display buffer read position (oldest sample)
    int readPos = (module->displayBufferPos + 1) % NamPlayer::DISPLAY_BUFFER_SIZE;
    
    // Draw each bar
    for (int barIdx = 0; barIdx < numBars; barIdx++) {
        float x = barIdx * barSpacing;
        
        // Calculate peak for this bar
        float peak = 0.0f;
        for (int i = 0; i < samplesPerBar; i++) {
            int sampleIdx = (readPos + barIdx * samplesPerBar + i) % NamPlayer::DISPLAY_BUFFER_SIZE;
            float sample = std::fabs(module->displayBuffer[sampleIdx]);
            peak = std::max(peak, sample);
        }
        
        // Apply logarithmic scaling for better visual distribution
        float barHeight = std::pow(peak, 0.6f) * maxBarHeight;
        barHeight = std::max(barHeight, 1.0f);  // Minimum bar height
        
        // Get user-selected color
        int r, g, b;
        module->getWaveformColorRGB(r, g, b);
        
        // Apply brightness based on peak level
        float brightness = 0.5f + peak * 0.5f;  // 50-100% brightness
        int r1 = static_cast<int>(r * brightness);
        int g1 = static_cast<int>(g * brightness);
        int b1 = static_cast<int>(b * brightness);
        int r2 = static_cast<int>(r * brightness * 0.6f);  // Darker for gradient
        int g2 = static_cast<int>(g * brightness * 0.6f);
        int b2 = static_cast<int>(b * brightness * 0.6f);
        
        // Create gradient from lighter at center to darker at edges
        NVGpaint gradient = nvgLinearGradient(args.vg,
            x, centerY - barHeight,
            x, centerY + barHeight,
            nvgRGBA(r1, g1, b1, 220),
            nvgRGBA(r2, g2, b2, 180));
        
        // Draw top bar (positive amplitude)
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, x, centerY - barHeight, kBarWidth, barHeight, 0.5f);
        nvgFillPaint(args.vg, gradient);
        nvgFill(args.vg);
        
        // Draw bottom bar (mirrored)
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, x, centerY, kBarWidth, barHeight, 0.5f);
        nvgFillPaint(args.vg, gradient);
        nvgFill(args.vg);
    }
}

Model* modelNamPlayer = createModel<NamPlayer, NamPlayerWidget>("NamPlayer");
