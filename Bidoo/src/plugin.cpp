#include "plugin.hpp"
// #include <iostream>

#if defined(METAMODULE_BUILTIN)
extern Plugin *pluginInstance;
#else
Plugin *pluginInstance;
#endif

#if defined(METAMODULE_BUILTIN)
void init_Bidoo(rack::Plugin *p) {
#else 
void init(rack::Plugin *p) {
#endif
	pluginInstance = p;

	p->addModel(modelACNE);
	p->addModel(modelBAFIS);
	p->addModel(modelBANCAU);
	p->addModel(modelBAR);
	p->addModel(modelBISTROT);
	p->addModel(modelBORDL);
	p->addModel(modelCANARD);
	p->addModel(modelCHUTE);
	p->addModel(modelDFUZE);
	p->addModel(modelDIKTAT);
	p->addModel(modelDILEMO);
	p->addModel(modelDTROY);
	p->addModel(modelDUKE);
	// p->addModel(modelENCORE);
	// // p->addModel(modelENCOREExpander);
	p->addModel(modelEDSAROS);
	// // p->addModel(modelEMILE);
	// p->addModel(modelFLAME);
	p->addModel(modelFORK);
	p->addModel(modelFREIN);
	p->addModel(modelHCTIP);
	p->addModel(modelHUITRE);
	p->addModel(modelLAMBDA);
	p->addModel(modelLATE);
	p->addModel(modelLIMBO);
	// // p->addModel(modelLIMONADE);
	p->addModel(modelLOURDE);
	// // p->addModel(modelMAGMA);
	p->addModel(modelMINIBAR);
	// p->addModel(modelMOIRE);
	p->addModel(modelMS);
	// // p->addModel(modelMU);
	// // p->addModel(modelOAI);
	p->addModel(modelOUAIVE);
	p->addModel(modelPERCO);
	// // p->addModel(modelPILOT);
	// // p->addModel(modelPOUPRE);
	p->addModel(modelRABBIT);
	// p->addModel(modelRATEAU);
	p->addModel(modelREI);
	p->addModel(modelSIGMA);
	p->addModel(modelSPORE);
	p->addModel(modelTIARE);
	p->addModel(modelTOCANTE);
	// // p->addModel(modelVOID);
	p->addModel(modelZINC);
	// p->addModel(modelZOUMAI);
	// p->addModel(modelZOUMAIExpander);
}

void InstantiateExpanderItem::onAction(const event::Action &e) {
	engine::Module* module = model->createModule();
	APP->engine->addModule(module);
	ModuleWidget* mw = model->createModuleWidget(module);
	if (mw) {
		APP->scene->rack->setModulePosNearest(mw, posit);
		APP->scene->rack->addModule(mw);
		history::ModuleAdd *h = new history::ModuleAdd;
		h->name = "create expander module";
		h->setModule(mw);
		APP->history->push(h);
	}
}

json_t *BidooModule::dataToJson() {
	json_t *rootJ = json_object();
	json_object_set_new(rootJ, "themeId", json_integer(themeId));
	return rootJ;
}

void BidooModule::dataFromJson(json_t *rootJ) {
	json_t *themeIdJ = json_object_get(rootJ, "themeId");
	if (themeIdJ)
		themeId = json_integer_value(themeIdJ);
}

void BidooWidget::appendContextMenu(Menu *menu) {
	ModuleWidget::appendContextMenu(menu);
	menu->addChild(new MenuSeparator());
	menu->addChild(createSubmenuItem("Theme", "", [=](ui::Menu* menu) {
		menu->addChild(construct<LightItem>(&MenuItem::text, dynamic_cast<BidooModule*>(module)->themeId == 0 ? "Light ✓" : "Light", &LightItem::module, dynamic_cast<BidooModule*>(module), &LightItem::pWidget, dynamic_cast<BidooWidget*>(this)));
		menu->addChild(construct<DarkItem>(&MenuItem::text, dynamic_cast<BidooModule*>(module)->themeId == 1 ? "Dark ✓" : "Dark", &DarkItem::module, dynamic_cast<BidooModule*>(module), &DarkItem::pWidget, dynamic_cast<BidooWidget*>(this)));
		menu->addChild(construct<BlackItem>(&MenuItem::text, dynamic_cast<BidooModule*>(module)->themeId == 2 ? "Black ✓" : "Black", &BlackItem::module, dynamic_cast<BidooModule*>(module), &BlackItem::pWidget, dynamic_cast<BidooWidget*>(this)));
		menu->addChild(construct<BlueItem>(&MenuItem::text, dynamic_cast<BidooModule*>(module)->themeId == 3 ? "Blue ✓" : "Blue", &BlueItem::module, dynamic_cast<BidooModule*>(module), &BlueItem::pWidget, dynamic_cast<BidooWidget*>(this)));
		menu->addChild(construct<GreenItem>(&MenuItem::text, dynamic_cast<BidooModule*>(module)->themeId == 4 ? "Green ✓" : "Green", &GreenItem::module, dynamic_cast<BidooModule*>(module), &GreenItem::pWidget, dynamic_cast<BidooWidget*>(this)));
	}));
}

unsigned int packedColor(int r, int g, int b, int a) {
	return r + (g << 8) + (b << 16) + (a << 24);
}

void BidooWidget::writeThemeAndContrastAsDefault() {
	json_t *settingsJ = json_object();

	// defaultPanelTheme
	json_object_set_new(settingsJ, "themeDefault", json_integer(defaultPanelTheme));

	std::string settingsFilename = asset::user("Bidoo.json");
	FILE *file = fopen(settingsFilename.c_str(), "w");
	if (file) {
		json_dumpf(settingsJ, file, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
		fclose(file);
	}
	json_decref(settingsJ);
}

void BidooWidget::readThemeAndContrastFromDefault() {
	std::string settingsFilename = asset::user("Bidoo.json");
	FILE *file = fopen(settingsFilename.c_str(), "r");
	if (!file) {
		defaultPanelTheme = 0;
		writeThemeAndContrastAsDefault();
		return;
	}
	json_error_t error;
	json_t *settingsJ = json_loadf(file, 0, &error);
	if (!settingsJ) {
		fclose(file);
		defaultPanelTheme = 0;
		writeThemeAndContrastAsDefault();
		return;
	}

	// defaultPanelTheme
	json_t *themeDefaultJ = json_object_get(settingsJ, "themeDefault");
	if (themeDefaultJ) {
		defaultPanelTheme = json_integer_value(themeDefaultJ);
	}
	else {
		defaultPanelTheme = 0;
	}

	fclose(file);
	json_decref(settingsJ);
	return;
}

void BidooWidget::prepareThemes(const std::string& filename) {
	readThemeAndContrastFromDefault();

	setPanel(APP->window->loadSvg(filename));
}

void BidooWidget::step() {
	// if (module) {
	// 	if ((dynamic_cast<BidooModule*>(module)->loadDefault) && (dynamic_cast<BidooModule*>(module)->themeId == -1)) {
	// 		dynamic_cast<BidooModule*>(module)->loadDefault = false;
	// 		readThemeAndContrastFromDefault();
	// 		dynamic_cast<BidooModule*>(module)->themeId = defaultPanelTheme;
	// 		if (defaultPanelTheme == 0) {
	// 			lightPanel->setVisible(true);
	// 			darkPanel->setVisible(false);
	// 			blackPanel->setVisible(false);
	// 			bluePanel->setVisible(false);
	// 			greenPanel->setVisible(false);
	// 		}
	// 		else if (defaultPanelTheme == 1) {
	// 			lightPanel->setVisible(false);
	// 			darkPanel->setVisible(true);
	// 			blackPanel->setVisible(false);
	// 			bluePanel->setVisible(false);
	// 			greenPanel->setVisible(false);
	// 		}
	// 		else if (defaultPanelTheme == 2){
	// 			lightPanel->setVisible(false);
	// 			darkPanel->setVisible(false);
	// 			blackPanel->setVisible(true);
	// 			bluePanel->setVisible(false);
	// 			greenPanel->setVisible(false);
	// 		}
	// 		else if (defaultPanelTheme == 3){
	// 			lightPanel->setVisible(false);
	// 			darkPanel->setVisible(false);
	// 			blackPanel->setVisible(false);
	// 			bluePanel->setVisible(true);
	// 			greenPanel->setVisible(false);
	// 		}
	// 		else {
	// 			lightPanel->setVisible(false);
	// 			darkPanel->setVisible(false);
	// 			blackPanel->setVisible(false);
	// 			bluePanel->setVisible(false);
	// 			greenPanel->setVisible(true);
	// 		}
	// 	}
	// 	else if (dynamic_cast<BidooModule*>(module)->themeChanged) {
	// 		dynamic_cast<BidooModule*>(module)->themeChanged = false;
	// 		if (dynamic_cast<BidooModule*>(module)->themeId == 0) {
	// 			lightPanel->setVisible(true);
	// 			darkPanel->setVisible(false);
	// 			blackPanel->setVisible(false);
	// 			bluePanel->setVisible(false);
	// 			greenPanel->setVisible(false);
	// 		}
	// 		else if (dynamic_cast<BidooModule*>(module)->themeId == 1) {
	// 			lightPanel->setVisible(false);
	// 			darkPanel->setVisible(true);
	// 			blackPanel->setVisible(false);
	// 			bluePanel->setVisible(false);
	// 			greenPanel->setVisible(false);
	// 		}
	// 		else if (dynamic_cast<BidooModule*>(module)->themeId == 2){
	// 			lightPanel->setVisible(false);
	// 			darkPanel->setVisible(false);
	// 			blackPanel->setVisible(true);
	// 			bluePanel->setVisible(false);
	// 			greenPanel->setVisible(false);
	// 		}
	// 		else if (dynamic_cast<BidooModule*>(module)->themeId == 3){
	// 			lightPanel->setVisible(false);
	// 			darkPanel->setVisible(false);
	// 			blackPanel->setVisible(false);
	// 			bluePanel->setVisible(true);
	// 			greenPanel->setVisible(false);
	// 		}
	// 		else {
	// 			lightPanel->setVisible(false);
	// 			darkPanel->setVisible(false);
	// 			blackPanel->setVisible(false);
	// 			bluePanel->setVisible(false);
	// 			greenPanel->setVisible(true);
	// 		}
	// 	}
	// }
	// else {
	// 	readThemeAndContrastFromDefault();
	// 	if (defaultPanelTheme == 0) {
	// 		lightPanel->setVisible(true);
	// 		darkPanel->setVisible(false);
	// 		blackPanel->setVisible(false);
	// 		bluePanel->setVisible(false);
	// 		greenPanel->setVisible(false);
	// 	}
	// 	else if (defaultPanelTheme == 1) {
	// 		lightPanel->setVisible(false);
	// 		darkPanel->setVisible(true);
	// 		blackPanel->setVisible(false);
	// 		bluePanel->setVisible(false);
	// 		greenPanel->setVisible(false);
	// 	}
	// 	else if (defaultPanelTheme == 2){
	// 		lightPanel->setVisible(false);
	// 		darkPanel->setVisible(false);
	// 		blackPanel->setVisible(true);
	// 		bluePanel->setVisible(false);
	// 		greenPanel->setVisible(false);
	// 	}
	// 	else if (defaultPanelTheme == 3){
	// 		lightPanel->setVisible(false);
	// 		darkPanel->setVisible(false);
	// 		blackPanel->setVisible(false);
	// 		bluePanel->setVisible(true);
	// 		greenPanel->setVisible(false);
	// 	}
	// 	else {
	// 		lightPanel->setVisible(false);
	// 		darkPanel->setVisible(false);
	// 		blackPanel->setVisible(false);
	// 		bluePanel->setVisible(false);
	// 		greenPanel->setVisible(true);
	// 	}
	// }
	// ModuleWidget::step();
}
