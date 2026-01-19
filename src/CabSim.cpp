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
    if (hasPendingSampleRate.exchange(false, std::memory_order_acq_rel)) {
        float newRate = pendingSampleRate.load(std::memory_order_acquire);
        if (cabSimDsp) {
            cabSimDsp->setSampleRate(newRate);
        }
        lastLpFreq = -1.f;
        lastHpFreq = -1.f;
    }
    if (cabSimDsp) {
        for (int slot = 0; slot < 2; ++slot) {
            if (hasPendingIrUnload[slot].exchange(false, std::memory_order_acq_rel)) {
                cabSimDsp->unloadIR(slot);
            }
            if (hasPendingIr[slot].exchange(false, std::memory_order_acq_rel)) {
                std::vector<float> samples = std::move(pendingIrSamples[slot]);
                std::string path = std::move(pendingIrPath[slot]);
                std::string name = std::move(pendingIrName[slot]);
                if (!samples.empty()) {
                    cabSimDsp->setIRKernel(slot, samples, path, name);
                }
            }
        }
    }
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

    // Update filter coefficients only when parameters change
    if (cabSimDsp) {
        if (std::abs(lpFreq - lastLpFreq) > 1.f || std::abs(hpFreq - lastHpFreq) > 1.f) {
            cabSimDsp->setFilterFrequencies(lpFreq, hpFreq);
            lastLpFreq = lpFreq;
            lastHpFreq = hpFreq;
        }
    }

    // Process through DSP
    float output = 0.f;
    if (cabSimDsp) {
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
    float newSampleRate = e.sampleRate;
    
    // Skip if sample rate hasn't actually changed
    if (std::abs(currentSampleRate.load(std::memory_order_acquire) - newSampleRate) < 1.f) {
        return;
    }
    
    currentSampleRate.store(newSampleRate, std::memory_order_release);
    pendingSampleRate.store(newSampleRate, std::memory_order_release);
    hasPendingSampleRate.store(true, std::memory_order_release);
    
    // Reload IRs at new sample rate if they were loaded
    // Store current paths since they'll be used by the background thread
    std::string pathA = irPathA;
    std::string pathB = irPathB;
    
    if (!pathA.empty() || !pathB.empty()) {
        // Wait for any previous load to finish first
        if (loadThread.joinable()) {
            loadThread.join();
        }
        
        isLoading = true;
        loadingSlot = -1;
        
        loadThread = std::thread([this, pathA, pathB]() {
            // Reload IR A if it was loaded
            if (!pathA.empty()) {
                loadingSlot = 0;
                try {
                    IRLoader loader;
                    if (loader.load(pathA)) {
                        loader.resampleTo(currentSampleRate.load(std::memory_order_acquire));
                        if (normalizeA) {
                            loader.normalize();
                        }
                        hasPendingIr[0].store(false, std::memory_order_release);
                        pendingIrSamples[0] = loader.getSamples();
                        pendingIrPath[0] = loader.getPath();
                        pendingIrName[0] = loader.getName();
                        hasPendingIr[0].store(true, std::memory_order_release);
                    }
                } catch (const std::exception& e) {
                    WARN("Exception reloading IR A at new sample rate: %s", e.what());
                }
            }
            
            // Reload IR B if it was loaded
            if (!pathB.empty()) {
                loadingSlot = 1;
                try {
                    IRLoader loader;
                    if (loader.load(pathB)) {
                        loader.resampleTo(currentSampleRate.load(std::memory_order_acquire));
                        if (normalizeB) {
                            loader.normalize();
                        }
                        hasPendingIr[1].store(false, std::memory_order_release);
                        pendingIrSamples[1] = loader.getSamples();
                        pendingIrPath[1] = loader.getPath();
                        pendingIrName[1] = loader.getName();
                        hasPendingIr[1].store(true, std::memory_order_release);
                    }
                } catch (const std::exception& e) {
                    WARN("Exception reloading IR B at new sample rate: %s", e.what());
                }
            }
            
            isLoading = false;
            loadingSlot = -1;
        });
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
                loader.resampleTo(currentSampleRate.load(std::memory_order_acquire));
                
                // Apply normalization if enabled
                bool shouldNormalize = (slot == 0) ? normalizeA : normalizeB;
                if (shouldNormalize) {
                    loader.normalize();
                }
                
                // Set the IR in DSP (thread-safe via mutex in process())
                hasPendingIr[slot].store(false, std::memory_order_release);
                pendingIrSamples[slot] = loader.getSamples();
                pendingIrPath[slot] = loader.getPath();
                pendingIrName[slot] = loader.getName();
                hasPendingIr[slot].store(true, std::memory_order_release);
                
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
    hasPendingIrUnload[slot].store(true, std::memory_order_release);
    
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
    if (!path.empty()) {
        // Wait for any existing load to complete before starting new load
        // This prevents race conditions when normalization settings change rapidly
        if (loadThread.joinable()) {
            loadThread.join();
        }
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
    
    // Extract IR paths for loading
    std::string pathA, pathB;
    
    json_t* pathAJ = json_object_get(rootJ, "irPathA");
    if (pathAJ) {
        pathA = json_string_value(pathAJ);
    }
    
    json_t* pathBJ = json_object_get(rootJ, "irPathB");
    if (pathBJ) {
        pathB = json_string_value(pathBJ);
    }
    
    // Load both IRs in a single background thread to avoid blocking UI
    if (!pathA.empty() || !pathB.empty()) {
        // Wait for any previous load to finish first
        if (loadThread.joinable()) {
            loadThread.join();
        }
        
        isLoading = true;
        loadingSlot = -1;  // Loading multiple slots
        
        loadThread = std::thread([this, pathA, pathB]() {
            // Load IR A if path exists
            if (!pathA.empty()) {
                loadingSlot = 0;
                try {
                    IRLoader loader;
                    if (loader.load(pathA)) {
                        loader.resampleTo(currentSampleRate.load(std::memory_order_acquire));
                        if (normalizeA) {
                            loader.normalize();
                        }
                        hasPendingIr[0].store(false, std::memory_order_release);
                        pendingIrSamples[0] = loader.getSamples();
                        pendingIrPath[0] = loader.getPath();
                        pendingIrName[0] = loader.getName();
                        hasPendingIr[0].store(true, std::memory_order_release);
                        irPathA = pathA;
                    } else {
                        WARN("Failed to load IR A: %s", pathA.c_str());
                    }
                } catch (const std::exception& e) {
                    WARN("Exception loading IR A: %s", e.what());
                }
            }
            
            // Load IR B if path exists
            if (!pathB.empty()) {
                loadingSlot = 1;
                try {
                    IRLoader loader;
                    if (loader.load(pathB)) {
                        loader.resampleTo(currentSampleRate.load(std::memory_order_acquire));
                        if (normalizeB) {
                            loader.normalize();
                        }
                        hasPendingIr[1].store(false, std::memory_order_release);
                        pendingIrSamples[1] = loader.getSamples();
                        pendingIrPath[1] = loader.getPath();
                        pendingIrName[1] = loader.getName();
                        hasPendingIr[1].store(true, std::memory_order_release);
                        irPathB = pathB;
                    } else {
                        WARN("Failed to load IR B: %s", pathB.c_str());
                    }
                } catch (const std::exception& e) {
                    WARN("Exception loading IR B: %s", e.what());
                }
            }
            
            isLoading = false;
            loadingSlot = -1;
        });
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
