#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
	pluginInstance = p;

	// Add modules here
	p->addModel(modelNamPlayer);
	p->addModel(modelNamFxLoop);
	p->addModel(modelCabSim);
	p->addModel(modelMnemonix);
}
