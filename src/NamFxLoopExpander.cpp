#include "NamFxLoopExpander.hpp"

#include <algorithm>
#include <cmath>

namespace {
float sanitizeVoltage(float voltage) {
    return std::isfinite(voltage) ? voltage : 0.f;
}
}

NamFxLoopExpander::NamFxLoopExpander() {
    config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

    configParam(BLEND_PARAM, 0.f, 1.f, 1.f, "Dry/Wet Blend", "%", 0.f, 100.f);
    configInput(RETURN_INPUT, "Return");
    configOutput(SEND_OUTPUT, "Send");
    configLight(LINK_LIGHT, "NAM Player link");

    leftExpander.producerMessage = new NamFxLoopToPlayerMessage;
    leftExpander.consumerMessage = new NamFxLoopToPlayerMessage;
}

NamFxLoopExpander::~NamFxLoopExpander() {
    delete static_cast<NamFxLoopToPlayerMessage*>(leftExpander.producerMessage);
    delete static_cast<NamFxLoopToPlayerMessage*>(leftExpander.consumerMessage);
}

void NamFxLoopExpander::process(const ProcessArgs& args) {
    (void) args;

    bool linkedToNamPlayer = false;
    float dryVoltage = 0.f;
    const bool connectedToNamPlayer = leftExpander.module && leftExpander.module->model == modelNamPlayer;

    if (connectedToNamPlayer) {
        auto* fromPlayer = static_cast<NamFxLoopToExpanderMessage*>(
            leftExpander.module->rightExpander.consumerMessage);
        if (fromPlayer && fromPlayer->active) {
            dryVoltage = sanitizeVoltage(fromPlayer->dryVoltage);
            linkedToNamPlayer = true;
        }
    }

    lights[LINK_LIGHT].setBrightness(connectedToNamPlayer ? 1.f : 0.f);
    outputs[SEND_OUTPUT].setVoltage(linkedToNamPlayer ? dryVoltage : 0.f);

    float outputVoltage = dryVoltage;
    if (linkedToNamPlayer) {
        float returnVoltage = inputs[RETURN_INPUT].isConnected()
            ? sanitizeVoltage(inputs[RETURN_INPUT].getVoltage())
            : dryVoltage;
        float blend = params[BLEND_PARAM].getValue();
        blend = std::max(0.f, std::min(1.f, blend));
        outputVoltage = dryVoltage + (returnVoltage - dryVoltage) * blend;
        if (!std::isfinite(outputVoltage)) {
            outputVoltage = dryVoltage;
        }
    }

    auto* toPlayer = static_cast<NamFxLoopToPlayerMessage*>(leftExpander.producerMessage);
    if (toPlayer) {
        toPlayer->outputVoltage = linkedToNamPlayer ? outputVoltage : 0.f;
        toPlayer->active = linkedToNamPlayer;
        leftExpander.requestMessageFlip();
    }
}

NamFxLoopExpanderWidget::NamFxLoopExpanderWidget(NamFxLoopExpander* module) {
    setModule(module);
    setPanel(createPanel(asset::plugin(pluginInstance, "res/NAM_FX_LOOP_PANEL.svg")));

    const float centerX = box.size.x / 2.f;

    addChild(createWidget<ScrewSilver>(Vec(0, 0)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - RACK_GRID_WIDTH, 0)));
    addChild(createWidget<ScrewSilver>(Vec(0, box.size.y - RACK_GRID_WIDTH)));
    addChild(createWidget<ScrewSilver>(Vec(box.size.x - RACK_GRID_WIDTH, box.size.y - RACK_GRID_WIDTH)));

    addParam(createParamCentered<RoundBlackKnob>(Vec(centerX, 112.f), module, NamFxLoopExpander::BLEND_PARAM));
    addChild(createLightCentered<SmallLight<GreenLight>>(Vec(centerX, 158.f), module, NamFxLoopExpander::LINK_LIGHT));
    addOutput(createOutputCentered<PJ301MPort>(Vec(centerX, 210.f), module, NamFxLoopExpander::SEND_OUTPUT));
    addInput(createInputCentered<PJ301MPort>(Vec(centerX, 286.f), module, NamFxLoopExpander::RETURN_INPUT));
}

Model* modelNamFxLoop = createModel<NamFxLoopExpander, NamFxLoopExpanderWidget>("NamFxLoop");
