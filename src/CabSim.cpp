#include "CabSim.hpp"
#include "dsp/IRLoader.h"
#include <osdialog.h>
#include <algorithm>

CabSim::CabSim() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
    
    // Blend: 0 = IR A only (or dry), 1 = IR B only (or wet)
    // When only one IR loaded, acts as wet/dry mix
    configParam(BLEND_PARAM, 0.f, 1.f, 0.5f, "IR Blend", "%", 0.f, 100.f);
    
    // Lowpass: 1kHz to 20kHz, log scale (2-pole, 12 dB/oct)
    // Display value = 1000 * 20^param, so param 0 = 1kHz, param 1 = 20kHz
    configParam(LOWPASS_PARAM, 0.f, 1.f, 1.f, "Low-Pass Cutoff", " Hz", 
                20.f, 1000.f);  // base=20, multiplier=1000 for exponential display
    
    // Highpass: 20Hz to 2kHz, log scale (2-pole, 12 dB/oct)
    // Display value = 20 * 100^param, so param 0 = 20Hz, param 1 = 2kHz
    configParam(HIGHPASS_PARAM, 0.f, 1.f, 0.f, "High-Pass Cutoff", " Hz",
                100.f, 20.f);  // base=100, multiplier=20 for exponential display
    
    // Output level: 0-200%
    configParam(OUTPUT_PARAM, 0.f, 2.f, 1.f, "Output Level", "%", 0.f, 100.f);
    
    configInput(AUDIO_INPUT, "Audio");
    configOutput(AUDIO_OUTPUT, "Audio");
    
    // Bypass configuration
    configBypass(AUDIO_INPUT, AUDIO_OUTPUT);
    
    // Initialize DSP
    cabSimDsp = std::make_unique<CabSimDSP>();
}

CabSim::~CabSim() {
    // Wait for any loading thread to finish
    if (loadThread.joinable()) {
        loadThread.join();
    }
}

void CabSim::process(const ProcessArgs& args) {
    // Get mono input (normalized to ±1.0)
    float input = inputs[AUDIO_INPUT].getVoltage() / 5.f;
    
    // Get parameters
    float blend = params[BLEND_PARAM].getValue();
    
    // Convert log params to Hz
    // LOWPASS: 1kHz to 20kHz -> param 0-1 maps to 1000 * 20^param
    float lpFreq = 1000.f * std::pow(20.f, params[LOWPASS_PARAM].getValue());
    // HIGHPASS: 20Hz to 2kHz -> param 0-1 maps to 20 * 100^param  
    float hpFreq = 20.f * std::pow(100.f, params[HIGHPASS_PARAM].getValue());
    
    float outputGain = params[OUTPUT_PARAM].getValue();
    
    // Process through DSP
    float output = 0.f;
    if (cabSimDsp) {
        std::lock_guard<std::mutex> lock(dspMutex);
        output = cabSimDsp->process(input, blend, lpFreq, hpFreq);
    } else {
        output = input;  // Passthrough if no DSP
    }
    
    // Apply output gain and convert back to ±5V
    outputs[AUDIO_OUTPUT].setVoltage(output * outputGain * 5.f);
    
    // Update lights
    bool irALoaded = cabSimDsp && cabSimDsp->isIRLoaded(0);
    bool irBLoaded = cabSimDsp && cabSimDsp->isIRLoaded(1);
    
    // Show loading state with pulsing light
    if (isLoading) {
        float pulse = 0.5f + 0.5f * std::sin(args.frame * 0.01f);
        if (loadingSlot == 0) {
            lights[IR_A_LIGHT].setBrightness(pulse);
        } else if (loadingSlot == 1) {
            lights[IR_B_LIGHT].setBrightness(pulse);
        }
    } else {
        lights[IR_A_LIGHT].setBrightness(irALoaded ? 1.f : 0.f);
        lights[IR_B_LIGHT].setBrightness(irBLoaded ? 1.f : 0.f);
    }
}

void CabSim::onSampleRateChange(const SampleRateChangeEvent& e) {
    currentSampleRate = e.sampleRate;
    
    std::lock_guard<std::mutex> lock(dspMutex);
    if (cabSimDsp) {
        cabSimDsp->setSampleRate(currentSampleRate);
        
        // Reload IRs at new sample rate if they were loaded
        // This is handled by storing paths and reloading
    }
}

void CabSim::loadIR(int slot, const std::string& path) {
    if (slot < 0 || slot > 1) return;
    
    // Don't start new load if already loading
    if (isLoading) {
        return;
    }
    
    // Wait for previous load to finish
    if (loadThread.joinable()) {
        loadThread.join();
    }
    
    isLoading = true;
    loadingSlot = slot;
    
    loadThread = std::thread([this, slot, path]() {
        try {
            IRLoader loader;
            
            if (loader.load(path)) {
                // Resample to current engine rate
                loader.resampleTo(currentSampleRate);
                
                // Apply normalization if enabled
                bool shouldNormalize = (slot == 0) ? normalizeA : normalizeB;
                if (shouldNormalize) {
                    loader.normalize();
                }
                
                // Set the IR in DSP (thread-safe via mutex in process())
                {
                    std::lock_guard<std::mutex> lock(dspMutex);
                    cabSimDsp->setIRKernel(slot, loader.getSamples(), 
                                           loader.getPath(), loader.getName());
                }
                
                // Store path for serialization
                if (slot == 0) {
                    irPathA = path;
                } else {
                    irPathB = path;
                }
            } else {
                WARN("Failed to load IR: %s", path.c_str());
            }
        } catch (const std::exception& e) {
            WARN("Exception loading IR: %s", e.what());
        }
        
        isLoading = false;
        loadingSlot = -1;
    });
}

void CabSim::unloadIR(int slot) {
    if (slot < 0 || slot > 1) return;
    
    std::lock_guard<std::mutex> lock(dspMutex);
    if (cabSimDsp) {
        cabSimDsp->unloadIR(slot);
    }
    
    if (slot == 0) {
        irPathA.clear();
    } else {
        irPathB.clear();
    }
}

void CabSim::setNormalize(int slot, bool enabled) {
    if (slot < 0 || slot > 1) return;
    
    if (slot == 0) {
        normalizeA = enabled;
    } else {
        normalizeB = enabled;
    }
    
    if (cabSimDsp) {
        cabSimDsp->setNormalize(slot, enabled);
    }
    
    // Reload IR with new normalization setting if one is loaded
    std::string path = (slot == 0) ? irPathA : irPathB;
    if (!path.empty() && !isLoading) {
        loadIR(slot, path);
    }
}

bool CabSim::getNormalize(int slot) const {
    if (slot == 0) return normalizeA;
    if (slot == 1) return normalizeB;
    return false;
}

std::string CabSim::getIRPath(int slot) const {
    if (slot == 0) return irPathA;
    if (slot == 1) return irPathB;
    return "";
}

std::string CabSim::getIRName(int slot) const {
    if (cabSimDsp) {
        return cabSimDsp->getIRName(slot);
    }
    return "";
}

bool CabSim::isIRLoaded(int slot) const {
    if (cabSimDsp) {
        return cabSimDsp->isIRLoaded(slot);
    }
    return false;
}

json_t* CabSim::dataToJson() {
    json_t* rootJ = json_object();
    
    // Save IR paths
    json_object_set_new(rootJ, "irPathA", json_string(irPathA.c_str()));
    json_object_set_new(rootJ, "irPathB", json_string(irPathB.c_str()));
    
    // Save normalization settings
    json_object_set_new(rootJ, "normalizeA", json_boolean(normalizeA));
    json_object_set_new(rootJ, "normalizeB", json_boolean(normalizeB));
    
    return rootJ;
}

void CabSim::dataFromJson(json_t* rootJ) {
    // Load normalization settings first (before loading IRs)
    json_t* normAJ = json_object_get(rootJ, "normalizeA");
    if (normAJ) {
        normalizeA = json_boolean_value(normAJ);
        if (cabSimDsp) cabSimDsp->setNormalize(0, normalizeA);
    }
    
    json_t* normBJ = json_object_get(rootJ, "normalizeB");
    if (normBJ) {
        normalizeB = json_boolean_value(normBJ);
        if (cabSimDsp) cabSimDsp->setNormalize(1, normalizeB);
    }
    
    // Load IR paths
    json_t* pathAJ = json_object_get(rootJ, "irPathA");
    if (pathAJ) {
        std::string path = json_string_value(pathAJ);
        if (!path.empty()) {
            // Wait for any previous load
            if (loadThread.joinable()) {
                loadThread.join();
            }
            loadIR(0, path);
            // Wait for this load to complete before loading B
            if (loadThread.joinable()) {
                loadThread.join();
            }
        }
    }
    
    json_t* pathBJ = json_object_get(rootJ, "irPathB");
    if (pathBJ) {
        std::string path = json_string_value(pathBJ);
        if (!path.empty()) {
            loadIR(1, path);
        }
    }
}

// Widget implementation
CabSimWidget::CabSimWidget(CabSim* module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/CabSim.svg")));

    // Module is 12HP = 60.96mm wide
    // Center X = 30.48mm
    
    // Screws (4 corners)
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    // Blend knob (large, centered at top)
    addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(30.48, 30)), module, CabSim::BLEND_PARAM));
    
    // Filter knobs (medium, side by side)
    addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15, 55)), module, CabSim::HIGHPASS_PARAM));
    addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(45, 55)), module, CabSim::LOWPASS_PARAM));
    
    // Output knob (medium, centered)
    addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(30.48, 80)), module, CabSim::OUTPUT_PARAM));

    // Input jack (left side, bottom)
    addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15, 105)), module, CabSim::AUDIO_INPUT));
    
    // Output jack (right side, bottom)
    addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(45, 105)), module, CabSim::AUDIO_OUTPUT));

    // IR Lights (above the I/O jacks)
    addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(15, 95)), module, CabSim::IR_A_LIGHT));
    addChild(createLightCentered<MediumLight<GreenLight>>(mm2px(Vec(45, 95)), module, CabSim::IR_B_LIGHT));
}

void CabSimWidget::appendContextMenu(Menu* menu) {
    CabSim* module = dynamic_cast<CabSim*>(this->module);
    if (!module) return;

    menu->addChild(new MenuSeparator());
    menu->addChild(createMenuLabel("Impulse Responses"));
    
    // IR A section
    menu->addChild(createMenuItem("Load IR A...", "", [=]() {
        char* path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, 
            osdialog_filters_parse("WAV files:wav"));
        if (path) {
            module->loadIR(0, path);
            free(path);
        }
    }));
    
    menu->addChild(createMenuItem("Unload IR A", "", [=]() {
        module->unloadIR(0);
    }, !module->isIRLoaded(0)));
    
    menu->addChild(createBoolMenuItem("Normalize IR A", "",
        [=]() { return module->getNormalize(0); },
        [=](bool value) { module->setNormalize(0, value); }
    ));
    
    // Show IR A info if loaded
    std::string nameA = module->getIRName(0);
    if (!nameA.empty()) {
        menu->addChild(createMenuLabel("  → " + nameA));
    }
    
    menu->addChild(new MenuSeparator());
    
    // IR B section
    menu->addChild(createMenuItem("Load IR B...", "", [=]() {
        char* path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, 
            osdialog_filters_parse("WAV files:wav"));
        if (path) {
            module->loadIR(1, path);
            free(path);
        }
    }));
    
    menu->addChild(createMenuItem("Unload IR B", "", [=]() {
        module->unloadIR(1);
    }, !module->isIRLoaded(1)));
    
    menu->addChild(createBoolMenuItem("Normalize IR B", "",
        [=]() { return module->getNormalize(1); },
        [=](bool value) { module->setNormalize(1, value); }
    ));
    
    // Show IR B info if loaded
    std::string nameB = module->getIRName(1);
    if (!nameB.empty()) {
        menu->addChild(createMenuLabel("  → " + nameB));
    }
    
    // Blend behavior explanation
    menu->addChild(new MenuSeparator());
    bool bothLoaded = module->isIRLoaded(0) && module->isIRLoaded(1);
    if (bothLoaded) {
        menu->addChild(createMenuLabel("Blend: A ← → B crossfade"));
    } else if (module->isIRLoaded(0) || module->isIRLoaded(1)) {
        menu->addChild(createMenuLabel("Blend: Dry ← → Wet"));
    }
}

Model* modelCabSim = createModel<CabSim, CabSimWidget>("CabSim");
