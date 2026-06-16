#include "Mnemonix.hpp"

#include <algorithm>
#include <cmath>

namespace {

float finiteClamp(float value, float minValue, float maxValue) {
    if (!std::isfinite(value)) {
        value = 0.f;
    }
    return clamp(value, minValue, maxValue);
}

float applyCv(float knob, float cv, float amount) {
    return finiteClamp(knob + (cv / 5.f) * amount, 0.f, 1.f);
}

float outputControlVoltage(float value) {
    return finiteClamp(value, 0.f, 1.f) * 10.f;
}

bool isUsableSampleRate(float sampleRate) {
    return std::isfinite(sampleRate) && sampleRate >= 8000.f;
}

struct MnemonixLabelOverlay : TransparentWidget {
    void drawLabel(const DrawArgs& args, Vec pos, const char* text, float size = 9.f, NVGcolor color = nvgRGB(28, 28, 30)) {
        nvgFontFaceId(args.vg, APP->window->uiFont->handle);
        nvgFontSize(args.vg, size);
        nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(args.vg, color);
        nvgText(args.vg, pos.x, pos.y, text, NULL);
    }

    void draw(const DrawArgs& args) override {
        const NVGcolor dark = nvgRGB(26, 28, 31);
        const NVGcolor soft = nvgRGB(80, 88, 96);
        const NVGcolor blue = nvgRGB(20, 92, 132);

        drawLabel(args, Vec(165, 18), "MNEMONIX", 18.f, dark);
        drawLabel(args, Vec(165, 35), "SHORTWAV LABS  BBD MEMORY DELAY", 8.f, soft);

        const char* mainLabels[] = {"LEVEL", "BLEND", "FEED", "DELAY", "DEPTH"};
        const float xs[] = {45.f, 105.f, 165.f, 225.f, 285.f};
        for (int i = 0; i < 5; ++i) {
            drawLabel(args, Vec(xs[i], 104), mainLabels[i], 8.5f, dark);
            drawLabel(args, Vec(xs[i], 132), "CV", 7.5f, soft);
            drawLabel(args, Vec(xs[i], 174), "IN", 7.5f, soft);
        }

        drawLabel(args, Vec(76, 216), "CHORUS", 8.f, dark);
        drawLabel(args, Vec(76, 227), "VIB", 8.f, dark);
        drawLabel(args, Vec(126, 216), "MODE", 8.f, soft);
        drawLabel(args, Vec(212, 216), "BYPASS", 8.f, dark);
        drawLabel(args, Vec(256, 216), "GATE", 8.f, soft);

        drawLabel(args, Vec(45, 286), "LVL", 7.5f, blue);
        drawLabel(args, Vec(85, 286), "BLD", 7.5f, blue);
        drawLabel(args, Vec(125, 286), "FB", 7.5f, blue);
        drawLabel(args, Vec(165, 286), "DLY", 7.5f, blue);
        drawLabel(args, Vec(205, 286), "DPT", 7.5f, blue);
        drawLabel(args, Vec(250, 286), "MOD", 7.5f, blue);
        drawLabel(args, Vec(290, 286), "ON", 7.5f, blue);
        drawLabel(args, Vec(165, 300), "CONTROL OUTS", 7.5f, soft);

        drawLabel(args, Vec(40, 356), "IN", 8.f, dark);
        drawLabel(args, Vec(105, 356), "DIRECT", 8.f, dark);
        drawLabel(args, Vec(165, 356), "WET", 8.f, dark);
        drawLabel(args, Vec(290, 356), "OUT", 8.f, dark);
    }
};

} // namespace

Mnemonix::Mnemonix() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

    configParam(LEVEL_PARAM, 0.f, 1.f, 0.55f, "Level", "%", 0.f, 100.f);
    configParam(BLEND_PARAM, 0.f, 1.f, 0.5f, "Blend", "%", 0.f, 100.f);
    configParam(FEEDBACK_PARAM, 0.f, 1.f, 0.25f, "Feedback", "%", 0.f, 100.f);
    configParam(DELAY_PARAM, 0.f, 1.f, 0.45f, "Delay", "%", 0.f, 100.f);
    configParam(DEPTH_PARAM, 0.f, 1.f, 0.2f, "Depth", "%", 0.f, 100.f);
    configSwitch(MODE_PARAM, 0.f, 1.f, 0.f, "Chorus/Vibrato", {"Chorus", "Vibrato"});
    configSwitch(ENGAGE_PARAM, 0.f, 1.f, 1.f, "Effect", {"Bypassed", "Engaged"});

    configParam(LEVEL_CV_ATT_PARAM, -1.f, 1.f, 0.f, "Level CV amount", "%", 0.f, 100.f);
    configParam(BLEND_CV_ATT_PARAM, -1.f, 1.f, 0.f, "Blend CV amount", "%", 0.f, 100.f);
    configParam(FEEDBACK_CV_ATT_PARAM, -1.f, 1.f, 0.f, "Feedback CV amount", "%", 0.f, 100.f);
    configParam(DELAY_CV_ATT_PARAM, -1.f, 1.f, 0.f, "Delay CV amount", "%", 0.f, 100.f);
    configParam(DEPTH_CV_ATT_PARAM, -1.f, 1.f, 0.f, "Depth CV amount", "%", 0.f, 100.f);

    configInput(AUDIO_INPUT, "Audio");
    configInput(LEVEL_CV_INPUT, "Level CV");
    configInput(BLEND_CV_INPUT, "Blend CV");
    configInput(FEEDBACK_CV_INPUT, "Feedback CV");
    configInput(DELAY_CV_INPUT, "Delay CV");
    configInput(DEPTH_CV_INPUT, "Depth CV");
    configInput(MODE_CV_INPUT, "Chorus/Vibrato Gate");
    configInput(ENGAGE_CV_INPUT, "Engage Gate");

    configOutput(AUDIO_OUTPUT, "Audio");
    configOutput(DIRECT_OUTPUT, "Direct");
    configOutput(WET_OUTPUT, "Wet");
    configOutput(LEVEL_CV_OUTPUT, "Level CV");
    configOutput(BLEND_CV_OUTPUT, "Blend CV");
    configOutput(FEEDBACK_CV_OUTPUT, "Feedback CV");
    configOutput(DELAY_CV_OUTPUT, "Delay CV");
    configOutput(DEPTH_CV_OUTPUT, "Depth CV");
    configOutput(MODE_GATE_OUTPUT, "Chorus/Vibrato Gate");
    configOutput(ENGAGE_GATE_OUTPUT, "Engage Gate");

    configBypass(AUDIO_INPUT, AUDIO_OUTPUT);
}

void Mnemonix::process(const ProcessArgs& args) {
    setEngineSampleRate(args.sampleRate);

    MnemonixDSP::Params dspParams;
    dspParams.level = applyCv(params[LEVEL_PARAM].getValue(),
        inputs[LEVEL_CV_INPUT].isConnected() ? inputs[LEVEL_CV_INPUT].getVoltage() : 0.f,
        params[LEVEL_CV_ATT_PARAM].getValue());
    dspParams.blend = applyCv(params[BLEND_PARAM].getValue(),
        inputs[BLEND_CV_INPUT].isConnected() ? inputs[BLEND_CV_INPUT].getVoltage() : 0.f,
        params[BLEND_CV_ATT_PARAM].getValue());
    dspParams.feedback = applyCv(params[FEEDBACK_PARAM].getValue(),
        inputs[FEEDBACK_CV_INPUT].isConnected() ? inputs[FEEDBACK_CV_INPUT].getVoltage() : 0.f,
        params[FEEDBACK_CV_ATT_PARAM].getValue());
    dspParams.delay = applyCv(params[DELAY_PARAM].getValue(),
        inputs[DELAY_CV_INPUT].isConnected() ? inputs[DELAY_CV_INPUT].getVoltage() : 0.f,
        params[DELAY_CV_ATT_PARAM].getValue());
    dspParams.depth = applyCv(params[DEPTH_PARAM].getValue(),
        inputs[DEPTH_CV_INPUT].isConnected() ? inputs[DEPTH_CV_INPUT].getVoltage() : 0.f,
        params[DEPTH_CV_ATT_PARAM].getValue());
    dspParams.vibrato = inputs[MODE_CV_INPUT].isConnected()
        ? inputs[MODE_CV_INPUT].getVoltage() >= 1.f
        : params[MODE_PARAM].getValue() >= 0.5f;
    dspParams.engaged = inputs[ENGAGE_CV_INPUT].isConnected()
        ? inputs[ENGAGE_CV_INPUT].getVoltage() >= 1.f
        : params[ENGAGE_PARAM].getValue() >= 0.5f;
    dspParams.artifactProfile = artifactProfile;

    const int channels = std::max(1, inputs[AUDIO_INPUT].getChannels());
    outputs[AUDIO_OUTPUT].setChannels(channels);
    outputs[DIRECT_OUTPUT].setChannels(channels);
    outputs[WET_OUTPUT].setChannels(channels);

    float overload = 0.f;
    for (int c = 0; c < channels; ++c) {
        const float in = inputs[AUDIO_INPUT].getPolyVoltage(c) / 5.f;
        const MnemonixDSP::Result result = engines[c].process(in, dspParams);

        const float out = std::isfinite(result.output) ? result.output : 0.f;
        const float direct = std::isfinite(result.direct) ? result.direct : 0.f;
        const float wet = std::isfinite(result.wet) ? result.wet : 0.f;

        outputs[AUDIO_OUTPUT].setVoltage(finiteClamp(out * 5.f, -12.f, 12.f), c);
        outputs[DIRECT_OUTPUT].setVoltage(finiteClamp(direct * 5.f, -12.f, 12.f), c);
        outputs[WET_OUTPUT].setVoltage(finiteClamp(wet * 5.f, -12.f, 12.f), c);
        overload = std::max(overload, result.overload);
    }

    outputs[LEVEL_CV_OUTPUT].setVoltage(outputControlVoltage(dspParams.level));
    outputs[BLEND_CV_OUTPUT].setVoltage(outputControlVoltage(dspParams.blend));
    outputs[FEEDBACK_CV_OUTPUT].setVoltage(outputControlVoltage(dspParams.feedback));
    outputs[DELAY_CV_OUTPUT].setVoltage(outputControlVoltage(dspParams.delay));
    outputs[DEPTH_CV_OUTPUT].setVoltage(outputControlVoltage(dspParams.depth));
    outputs[MODE_GATE_OUTPUT].setVoltage(dspParams.vibrato ? 10.f : 0.f);
    outputs[ENGAGE_GATE_OUTPUT].setVoltage(dspParams.engaged ? 10.f : 0.f);

    lights[OVERLOAD_LIGHT].setBrightnessSmooth(overload, args.sampleTime);
    lights[STATUS_LIGHT].setBrightnessSmooth(dspParams.engaged ? 1.f : 0.f, args.sampleTime);
    lights[VIBRATO_LIGHT].setBrightnessSmooth(dspParams.vibrato ? 1.f : 0.f, args.sampleTime);
}

void Mnemonix::onSampleRateChange(const SampleRateChangeEvent& e) {
    setEngineSampleRate(e.sampleRate);
}

json_t* Mnemonix::dataToJson() {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "artifactProfile", json_integer(artifactProfile));
    return rootJ;
}

void Mnemonix::dataFromJson(json_t* rootJ) {
    json_t* artifactJ = json_object_get(rootJ, "artifactProfile");
    if (artifactJ && json_is_integer(artifactJ)) {
        setArtifactProfile(static_cast<int>(json_integer_value(artifactJ)));
    }
}

void Mnemonix::resetEngines() {
    for (auto& engine : engines) {
        engine.reset();
    }
}

void Mnemonix::setArtifactProfile(int profile) {
    if (profile < MnemonixDSP::ARTIFACT_CLEAN || profile > MnemonixDSP::ARTIFACT_WORN) {
        profile = MnemonixDSP::ARTIFACT_AUTHENTIC;
    }
    artifactProfile = profile;
}

void Mnemonix::setEngineSampleRate(float sampleRate) {
    if (!isUsableSampleRate(sampleRate)) {
        return;
    }

    if (sampleRateInitialized && std::fabs(sampleRate - currentSampleRate) < 0.01f) {
        return;
    }

    currentSampleRate = sampleRate;
    sampleRateInitialized = true;
    for (auto& engine : engines) {
        engine.setSampleRate(currentSampleRate);
    }
}

MnemonixWidget::MnemonixWidget(Mnemonix* module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/MNEMONIX_PANEL.svg")));

    addChild(createWidget<ScrewSilver>(Vec(0, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

    MnemonixLabelOverlay* labels = new MnemonixLabelOverlay();
    labels->box.pos = Vec(0, 0);
    labels->box.size = box.size;
    addChild(labels);

    const float xs[] = {45.f, 105.f, 165.f, 225.f, 285.f};
    addParam(createParamCentered<Davies1900hLargeBlackKnob>(Vec(xs[0], 72), module, Mnemonix::LEVEL_PARAM));
    addParam(createParamCentered<Davies1900hLargeBlackKnob>(Vec(xs[1], 72), module, Mnemonix::BLEND_PARAM));
    addParam(createParamCentered<Davies1900hLargeBlackKnob>(Vec(xs[2], 72), module, Mnemonix::FEEDBACK_PARAM));
    addParam(createParamCentered<Davies1900hLargeBlackKnob>(Vec(xs[3], 72), module, Mnemonix::DELAY_PARAM));
    addParam(createParamCentered<Davies1900hLargeBlackKnob>(Vec(xs[4], 72), module, Mnemonix::DEPTH_PARAM));

    addParam(createParamCentered<Trimpot>(Vec(xs[0], 124), module, Mnemonix::LEVEL_CV_ATT_PARAM));
    addParam(createParamCentered<Trimpot>(Vec(xs[1], 124), module, Mnemonix::BLEND_CV_ATT_PARAM));
    addParam(createParamCentered<Trimpot>(Vec(xs[2], 124), module, Mnemonix::FEEDBACK_CV_ATT_PARAM));
    addParam(createParamCentered<Trimpot>(Vec(xs[3], 124), module, Mnemonix::DELAY_CV_ATT_PARAM));
    addParam(createParamCentered<Trimpot>(Vec(xs[4], 124), module, Mnemonix::DEPTH_CV_ATT_PARAM));

    addInput(createInputCentered<PJ301MPort>(Vec(xs[0], 158), module, Mnemonix::LEVEL_CV_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(xs[1], 158), module, Mnemonix::BLEND_CV_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(xs[2], 158), module, Mnemonix::FEEDBACK_CV_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(xs[3], 158), module, Mnemonix::DELAY_CV_INPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(xs[4], 158), module, Mnemonix::DEPTH_CV_INPUT));

    addParam(createParamCentered<CKSS>(Vec(76, 202), module, Mnemonix::MODE_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(126, 202), module, Mnemonix::MODE_CV_INPUT));
    addParam(createParamCentered<CKSS>(Vec(212, 202), module, Mnemonix::ENGAGE_PARAM));
    addInput(createInputCentered<PJ301MPort>(Vec(256, 202), module, Mnemonix::ENGAGE_CV_INPUT));

    addChild(createLightCentered<MediumLight<RedLight>>(Vec(45, 226), module, Mnemonix::OVERLOAD_LIGHT));
    addChild(createLightCentered<MediumLight<GreenLight>>(Vec(290, 226), module, Mnemonix::STATUS_LIGHT));
    addChild(createLightCentered<SmallLight<YellowLight>>(Vec(156, 202), module, Mnemonix::VIBRATO_LIGHT));

    addOutput(createOutputCentered<PJ301MPort>(Vec(45, 268), module, Mnemonix::LEVEL_CV_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(85, 268), module, Mnemonix::BLEND_CV_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(125, 268), module, Mnemonix::FEEDBACK_CV_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(165, 268), module, Mnemonix::DELAY_CV_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(205, 268), module, Mnemonix::DEPTH_CV_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(250, 268), module, Mnemonix::MODE_GATE_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(290, 268), module, Mnemonix::ENGAGE_GATE_OUTPUT));

    addInput(createInputCentered<PJ301MPort>(Vec(40, 334), module, Mnemonix::AUDIO_INPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(105, 334), module, Mnemonix::DIRECT_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(165, 334), module, Mnemonix::WET_OUTPUT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(290, 334), module, Mnemonix::AUDIO_OUTPUT));
}

void MnemonixWidget::appendContextMenu(Menu* menu) {
    Mnemonix* mnemonix = dynamic_cast<Mnemonix*>(module);
    if (!mnemonix) {
        return;
    }

    menu->addChild(new MenuSeparator());
    menu->addChild(createMenuLabel("Mnemonix Calibration"));

    struct ProfileOption {
        int value;
        const char* label;
    };

    const ProfileOption profiles[] = {
        {MnemonixDSP::ARTIFACT_CLEAN, "Cleaner BBD"},
        {MnemonixDSP::ARTIFACT_AUTHENTIC, "Rev_D authentic"},
        {MnemonixDSP::ARTIFACT_WORN, "Worn unit"}
    };

    menu->addChild(createSubmenuItem("Artifact profile", "", [=](Menu* submenu) {
        for (const auto& profile : profiles) {
            submenu->addChild(createMenuItem(profile.label,
                mnemonix->artifactProfile == profile.value ? "*" : "",
                [=]() { mnemonix->setArtifactProfile(profile.value); }));
        }
    }));

    menu->addChild(createMenuItem("Clear delay memory", "", [=]() {
        mnemonix->resetEngines();
    }));
}

Model* modelMnemonix = createModel<Mnemonix, MnemonixWidget>("Mnemonix");
