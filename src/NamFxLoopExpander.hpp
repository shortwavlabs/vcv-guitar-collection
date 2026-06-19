#pragma once

#include "plugin.hpp"

struct NamFxLoopToExpanderMessage {
    float dryVoltage = 0.f;
    bool active = false;
};

struct NamFxLoopToPlayerMessage {
    float outputVoltage = 0.f;
    bool active = false;
};

struct NamFxLoopExpander : Module {
    enum ParamId {
        BLEND_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        RETURN_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        SEND_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        LINK_LIGHT,
        LIGHTS_LEN
    };

    NamFxLoopExpander();
    ~NamFxLoopExpander();

    void process(const ProcessArgs& args) override;
};

struct NamFxLoopExpanderWidget : ModuleWidget {
    NamFxLoopExpanderWidget(NamFxLoopExpander* module);
};
