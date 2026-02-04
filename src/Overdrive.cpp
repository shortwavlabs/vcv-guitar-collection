#include "Overdrive.hpp"
#include <algorithm>
#include <cmath>

Overdrive::Overdrive() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

    configSwitch(MODEL_PARAM, 0.f, 2.f, 0.f, "Model", {"TS-808", "TS-9", "DS-1"});
    configParam(DRIVE_PARAM, 0.f, 1.f, 0.5f, "Drive");
    configParam(TONE_PARAM, 0.f, 1.f, 0.5f, "Tone");
    configParam(LEVEL_PARAM, 0.f, 1.f, 0.8f, "Level");
    configParam(ATTACK_PARAM, 0.f, 5.f, 3.f, "Attack", " position");
    configParam(GATE_PARAM, 0.f, 1.f, 0.f, "Gate");
    configButton(BYPASS_PARAM, "Bypass");

    configInput(AUDIO_INPUT, "Audio");
    configOutput(AUDIO_OUTPUT, "Audio");

    configInput(DRIVE_CV, "Drive CV");
    configInput(TONE_CV, "Tone CV");
    configInput(LEVEL_CV, "Level CV");
    configInput(ATTACK_CV, "Attack CV");
    configInput(GATE_CV, "Gate CV");

    configBypass(AUDIO_INPUT, AUDIO_OUTPUT);

    driveSmoother.setTau(0.01f);
    toneSmoother.setTau(0.01f);
    levelSmoother.setTau(0.01f);
    gateSmoother.setTau(0.01f);
}

void Overdrive::process(const ProcessArgs& args) {
    int modelInt = static_cast<int>(std::round(params[MODEL_PARAM].getValue()));
    if (modelInt < 0) modelInt = 0;
    if (modelInt > 2) modelInt = 2;
    if (modelInt != cachedModel) {
        dsp.setModel(static_cast<OverdriveModel>(modelInt));
        cachedModel = modelInt;
    }

    float drive = params[DRIVE_PARAM].getValue();
    if (inputs[DRIVE_CV].isConnected()) {
        float cv = inputs[DRIVE_CV].getVoltage();
        drive = rescale(cv, -5.f, 5.f, 0.f, 1.f);
    }
    drive = driveSmoother.process(args.sampleTime, drive);
    dsp.setDrive(drive);

    float tone = params[TONE_PARAM].getValue();
    if (inputs[TONE_CV].isConnected()) {
        float cv = inputs[TONE_CV].getVoltage();
        tone = rescale(cv, -5.f, 5.f, 0.f, 1.f);
    }
    tone = toneSmoother.process(args.sampleTime, tone);
    dsp.setTone(tone);

    float level = params[LEVEL_PARAM].getValue();
    if (inputs[LEVEL_CV].isConnected()) {
        float cv = inputs[LEVEL_CV].getVoltage();
        level = rescale(cv, -5.f, 5.f, 0.f, 1.f);
    }
    level = levelSmoother.process(args.sampleTime, level);
    dsp.setLevel(level);

    int attack = static_cast<int>(std::round(params[ATTACK_PARAM].getValue()));
    if (inputs[ATTACK_CV].isConnected()) {
        float cv = std::clamp(inputs[ATTACK_CV].getVoltage(), 0.f, 10.f);
        int cvPos = static_cast<int>(std::round(rescale(cv, 0.f, 10.f, 0.f, 5.f)));
        attack = std::clamp(attack + cvPos, 0, 5);
    }
    if (attack != cachedAttack) {
        dsp.setAttack(attack);
        cachedAttack = attack;
    }

    float gate = params[GATE_PARAM].getValue();
    if (inputs[GATE_CV].isConnected()) {
        float cv = inputs[GATE_CV].getVoltage();
        gate = rescale(cv, -5.f, 5.f, 0.f, 1.f);
    }
    gate = gateSmoother.process(args.sampleTime, gate);
    dsp.setGate(gate);

    float inputVoltage = inputs[AUDIO_INPUT].getVoltage();
    float input = inputVoltage / 5.f;

    if (bypassTrigger.process(params[BYPASS_PARAM].getValue())) {
        isBypassed = !isBypassed;
    }

    bool bypassed = isBypassed;
    if (bypassed) {
        outputs[AUDIO_OUTPUT].setVoltage(inputVoltage);
    } else {
        float output = dsp.process(input);
        outputs[AUDIO_OUTPUT].setVoltage(output * 5.f);
    }

    bool gateOpen = dsp.isGateOpen();
    if (!lightStateInitialized || gateOpen != cachedGateOpen || bypassed != cachedBypassed) {
        if (bypassed) {
            lights[GATE_GREEN_LIGHT].setBrightness(0.f);
            lights[GATE_RED_LIGHT].setBrightness(0.f);
        } else if (gateOpen) {
            lights[GATE_GREEN_LIGHT].setBrightness(1.f);
            lights[GATE_RED_LIGHT].setBrightness(0.f);
        } else {
            lights[GATE_GREEN_LIGHT].setBrightness(0.f);
            lights[GATE_RED_LIGHT].setBrightness(1.f);
        }
        cachedGateOpen = gateOpen;
        cachedBypassed = bypassed;
        lightStateInitialized = true;
    }
}

void Overdrive::onSampleRateChange(const SampleRateChangeEvent& e) {
    dsp.setSampleRate(e.sampleRate);
}

void Overdrive::onReset(const ResetEvent& e) {
    (void)e;
    dsp.reset();
    isBypassed = false;
    bypassTrigger.reset();
    cachedModel = -1;
    cachedAttack = -1;
    cachedGateOpen = false;
    cachedBypassed = false;
    lightStateInitialized = false;
}

json_t* Overdrive::dataToJson() {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "bypass", json_boolean(isBypassed));
    return rootJ;
}

void Overdrive::dataFromJson(json_t* rootJ) {
    json_t* bypassJ = json_object_get(rootJ, "bypass");
    if (bypassJ) {
        isBypassed = json_is_true(bypassJ);
    }
}

OverdriveWidget::OverdriveWidget(Overdrive* module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/OVERDRIVE_PANEL.svg")));

    float centerX = box.size.x / 2.0f;

    addChild(createWidget<ScrewSilver>(Vec(0, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - 1 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    addParam(createParamCentered<CKSSThree>(Vec(centerX, 35), module, Overdrive::MODEL_PARAM));

    addParam(createParamCentered<RoundBlackKnob>(Vec(centerX, 70), module, Overdrive::DRIVE_PARAM));
    addParam(createParamCentered<RoundBlackKnob>(Vec(centerX, 110), module, Overdrive::TONE_PARAM));
    addParam(createParamCentered<RoundBlackKnob>(Vec(centerX, 150), module, Overdrive::LEVEL_PARAM));
    addParam(createParamCentered<RoundBlackSnapKnob>(Vec(centerX, 190), module, Overdrive::ATTACK_PARAM));
    addParam(createParamCentered<RoundSmallBlackKnob>(Vec(centerX, 225), module, Overdrive::GATE_PARAM));
    addParam(createParamCentered<LEDButton>(Vec(centerX, 255), module, Overdrive::BYPASS_PARAM));

    addInput(createInputCentered<PJ301MPort>(Vec(20, 260), module, Overdrive::AUDIO_INPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(box.size.x - 20, 260), module, Overdrive::AUDIO_OUTPUT));

    float cvStartY = 280.f;
    float cvSpacing = 25.f;
    addInput(createInputCentered<PJ301MPort>(Vec(20, cvStartY), module, Overdrive::DRIVE_CV));
    addInput(createInputCentered<PJ301MPort>(Vec(20, cvStartY + cvSpacing), module, Overdrive::TONE_CV));
    addInput(createInputCentered<PJ301MPort>(Vec(20, cvStartY + cvSpacing * 2), module, Overdrive::LEVEL_CV));
    addInput(createInputCentered<PJ301MPort>(Vec(box.size.x - 20, cvStartY), module, Overdrive::ATTACK_CV));
    addInput(createInputCentered<PJ301MPort>(Vec(box.size.x - 20, cvStartY + cvSpacing), module, Overdrive::GATE_CV));

    addChild(createLightCentered<SmallLight<GreenRedLight>>(Vec(centerX, 245), module, Overdrive::GATE_GREEN_LIGHT));
}

Model* modelOverdrive = createModel<Overdrive, OverdriveWidget>("Overdrive");
