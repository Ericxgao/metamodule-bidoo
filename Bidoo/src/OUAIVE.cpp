#include "plugin.hpp"
#include "dsp/digital.hpp"
#include "BidooComponents.hpp"
#include <vector>
#include <cmath>
#include <mutex>
#include "dep/waves.hpp"
#include <algorithm> // For std::min
#include <atomic> // For std::atomic

#if defined(METAMODULE)
#include "async_filebrowser.hh"
#include "CoreModules/async_thread.hh"
#else
#include "osdialog.h"
#endif

using namespace rack;
using namespace std;

struct OUAIVE : BidooModule {
	enum ParamIds {
		NB_SLICES_PARAM,
		TRIG_MODE_PARAM,
		READ_MODE_PARAM,
		SPEED_PARAM,
		CVSLICES_PARAM,
		CVSPEED_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		GATE_INPUT,
		POS_INPUT,
		NB_SLICES_INPUT,
		READ_MODE_INPUT,
		SPEED_INPUT,
		POS_RESET_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		OUTL_OUTPUT,
		OUTR_OUTPUT,
		EOC_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	bool play = false;
	int channels;
  int sampleRate;
  int totalSampleCount=0;
	float samplePos = 0.0f;
	vector<dsp::Frame<2>> playBuffer;
	std::string lastPath;
	std::string waveFileName;
	std::string waveExtension;
	bool loading = false;
	int trigMode = 0; // 0 trig 1 gate, 2 sliced
	int sliceIndex = -1;
	int sliceLength = 0;
	int nbSlices = 1;
	int readMode = 0; // 0 formward, 1 backward, 2 repeat
	float speed;
	dsp::SchmittTrigger playTrigger;
	dsp::SchmittTrigger trigModeTrigger;
	dsp::SchmittTrigger readModeTrigger;
	dsp::SchmittTrigger posResetTrigger;
	std::atomic<bool> locked{false};
	bool first = true;
	int eoc=0;
	bool pulse = false;
	dsp::PulseGenerator eocPulse;
	
#if defined(METAMODULE)
	MetaModule::AsyncThread loadSampleAsync{this, [this]() {
		this->loadSampleInternal();
	}};
#endif

	OUAIVE() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(TRIG_MODE_PARAM, 0.0, 2.0, 0.0, "Trigger mode");
		configParam(OUAIVE::READ_MODE_PARAM, 0.0, 2.0, 0.0, "Read mode");
		configParam(NB_SLICES_PARAM, 1.0, 128.01, 1.0, "Number of slices");
		configParam(CVSLICES_PARAM, -1.0f, 1.0f, 0.0f, "CV slices");
		configParam(SPEED_PARAM, -0.05, 10, 1.0, "Speed");
		configParam(CVSPEED_PARAM, -1.0f, 1.0f, 0.0f, "CV speed");

		configInput(GATE_INPUT, "Gate");
		configInput(POS_INPUT, "Position");
		configInput(NB_SLICES_INPUT, "Nb slices");
		configInput(READ_MODE_INPUT, "Read mode");
		configInput(SPEED_INPUT, "Speed");
		configInput(POS_RESET_INPUT, "Position reset");

		configOutput(OUTL_OUTPUT, "Out L");
		configOutput(OUTR_OUTPUT, "Out R");
		configOutput(EOC_OUTPUT, "EOC");

		playBuffer.resize(0);
	}

	void process(const ProcessArgs &args) override;

	void loadSample();
	void loadSampleInternal();

	void lock() {
		bool expected = false;
		while (!locked.compare_exchange_strong(expected, true)) {
			expected = false;
		}
	}

	void unlock() {
		locked.store(false);
	}

	json_t *dataToJson() override {
		json_t *rootJ = BidooModule::dataToJson();
		json_object_set_new(rootJ, "lastPath", json_string(lastPath.c_str()));
		json_object_set_new(rootJ, "trigMode", json_integer(trigMode));
		json_object_set_new(rootJ, "readMode", json_integer(readMode));
		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		BidooModule::dataFromJson(rootJ);
		json_t *lastPathJ = json_object_get(rootJ, "lastPath");
		if (lastPathJ) {
			lastPath = json_string_value(lastPathJ);
			if (!lastPath.empty()) loadSample();
		}
		json_t *trigModeJ = json_object_get(rootJ, "trigMode");
		if (trigModeJ) {
			trigMode = json_integer_value(trigModeJ);
		}
		json_t *readModeJ = json_object_get(rootJ, "readMode");
		if (readModeJ) {
			readMode = json_integer_value(readModeJ);
		}
	}

	void onSampleRateChange() override {
		if (!lastPath.empty()) loadSample();
	}
};

void OUAIVE::loadSampleInternal() {
	if (lastPath.empty()) {
		loading = false;
		return;
	}

	APP->engine->yieldWorkers();
	lock();
	playBuffer = waves::getStereoWav(lastPath, APP->engine->getSampleRate(), 
		waveFileName, waveExtension, channels, sampleRate, totalSampleCount);
	unlock();
	loading = false;

	vector<dsp::Frame<2>>(playBuffer).swap(playBuffer);
}

void OUAIVE::loadSample() {
#if defined(METAMODULE)
	loadSampleAsync.run_once();
#else
	loadSampleInternal();
#endif
}

void OUAIVE::process(const ProcessArgs &args) {
	if (loading) {
		loadSample();
	}
	if (trigModeTrigger.process(roundf(params[TRIG_MODE_PARAM].getValue()))) {
		trigMode = (((int)trigMode + 1) % 3);
	}
	if (inputs[READ_MODE_INPUT].isConnected()) {
		readMode = round(rescale(inputs[READ_MODE_INPUT].getVoltage(), 0.0f,10.0f,0.0f,2.0f));
	} else if (readModeTrigger.process(roundf(params[READ_MODE_PARAM].getValue() + inputs[READ_MODE_INPUT].getVoltage()))) {
		readMode = (((int)readMode + 1) % 3);
	}
	nbSlices = clamp((int)round(params[NB_SLICES_PARAM].getValue() + params[CVSLICES_PARAM].getValue() * inputs[NB_SLICES_INPUT].getVoltage()), 1, 128);
	speed = clamp(params[SPEED_PARAM].getValue() + params[CVSPEED_PARAM].getValue() * inputs[SPEED_INPUT].getVoltage(), 0.2f, 10.0f);

	sliceLength = clamp(totalSampleCount / nbSlices, 1, totalSampleCount);

	if ((trigMode == 0) && (playTrigger.process(inputs[GATE_INPUT].getVoltage()))) {
		play = true;
		if (inputs[POS_INPUT].isConnected())
			samplePos = clamp(inputs[POS_INPUT].getVoltage() * (totalSampleCount-1.0f) * 0.1f, 0.0f , totalSampleCount - 1.0f);
		else {
			if (readMode != 1)
				samplePos = 0.0f;
			else
				samplePos = totalSampleCount - 1.0f;
		}
	}	else if (trigMode == 1) {
		play = (inputs[GATE_INPUT].getVoltage() > 0.1f);
		samplePos = clamp(inputs[POS_INPUT].getVoltage() * (totalSampleCount-1.0f) * 0.1f, 0.0f , totalSampleCount - 1.0f);
	} else if ((trigMode == 2) && (playTrigger.process(inputs[GATE_INPUT].getVoltage()))) {
		play = true;
		if (inputs[POS_INPUT].isConnected())
			sliceIndex = clamp((int)(inputs[POS_INPUT].getVoltage() * nbSlices * 0.1f), 0, nbSlices);
		 else
			sliceIndex = (sliceIndex+1)%nbSlices;
		if (readMode != 1)
			samplePos = clamp(sliceIndex*sliceLength, 0, totalSampleCount-1);
		else
			samplePos = clamp((sliceIndex + 1) * sliceLength - 1, 0 , totalSampleCount-1);
	}

	if (posResetTrigger.process(inputs[POS_RESET_INPUT].getVoltage())) {
		sliceIndex = 0;
		samplePos = 0.0f;
	}

	if (play && (samplePos>=0) && (samplePos < totalSampleCount)) {
		int xi = static_cast<int>(samplePos);
		float xf = samplePos - xi;
        
		if (xi < playBuffer.size()) {
			// Acquire lock only once before processing both channels
			lock();
			
			if (channels == 1) {
				// Mono processing
				float nextSample = (xi + 1 < playBuffer.size()) ? 
					playBuffer[xi + 1].samples[0] : 
					playBuffer[xi].samples[0];
				float crossfaded = crossfade(playBuffer[xi].samples[0], nextSample, xf);
				
				// Set both outputs with the same value
				float outputVoltage = 5.0f * crossfaded;
				unlock();
				outputs[OUTL_OUTPUT].setVoltage(outputVoltage);
				outputs[OUTR_OUTPUT].setVoltage(outputVoltage);
			}
			else if (channels == 2) {
				// Stereo processing
				// Get sample data for both channels while holding the lock only once
				float sample0L = playBuffer[xi].samples[0];
				float sample0R = playBuffer[xi].samples[1];
				
				float sample1L = (xi + 1 < playBuffer.size()) ? playBuffer[xi + 1].samples[0] : sample0L;
				float sample1R = (xi + 1 < playBuffer.size()) ? playBuffer[xi + 1].samples[1] : sample0R;
				unlock();
				
				if (outputs[OUTL_OUTPUT].isConnected() && outputs[OUTR_OUTPUT].isConnected()) {
					// Both outputs connected - process as stereo
					float crossfadedL = crossfade(sample0L, sample1L, xf);
					float crossfadedR = crossfade(sample0R, sample1R, xf);
					
					outputs[OUTL_OUTPUT].setVoltage(5.0f * crossfadedL);
					outputs[OUTR_OUTPUT].setVoltage(5.0f * crossfadedR);
				}
				else {
					// Mix down to mono
					float crossfaded = crossfade(
						0.5f * (sample0L + sample0R),
						0.5f * (sample1L + sample1R),
						xf);
					
					outputs[OUTL_OUTPUT].setVoltage(5.0f * crossfaded);
					outputs[OUTR_OUTPUT].setVoltage(5.0f * crossfaded);
				}
			}
		}

		if (trigMode == 0) {
			if (readMode != 1)
				samplePos = samplePos + speed;
			else
				samplePos = samplePos - speed;
			//manage eof readMode
			if ((readMode == 0) && (samplePos >= totalSampleCount))
					play = false;
			else if ((readMode == 1) && (samplePos <=0))
					play = false;
			else if ((readMode == 2) && (samplePos >= totalSampleCount))
				samplePos = clamp(inputs[POS_INPUT].getVoltage() * (totalSampleCount-1.0f) * 0.1f , 0.0f ,totalSampleCount - 1.0f);
		}
		else if (trigMode == 2)
		{
			if (readMode != 1)
				samplePos = samplePos + speed;
			else
				samplePos = samplePos - speed;

			//manage eof readMode
			if ((readMode == 0) && ((samplePos >= (sliceIndex+1) * sliceLength) || (samplePos >= totalSampleCount)))
					play = false;
			if ((readMode == 1) && ((samplePos <= (sliceIndex) * sliceLength) || (samplePos <= 0)))
					play = false;
			if ((readMode == 2) && ((samplePos >= (sliceIndex+1) * sliceLength) || (samplePos >= totalSampleCount)))
				samplePos = clamp(sliceIndex*sliceLength, 0 , totalSampleCount);
		}
	}
	else if (samplePos == totalSampleCount) {
		play = false;
	}

	if ((eoc == 0) && (play)) {
		eoc = 1;
	}

	if ((eoc == 1) && (!play)) {
		eoc = 0;
		eocPulse.reset();
		eocPulse.trigger(1e-3f);
	}

	pulse = eocPulse.process(args.sampleTime);

	outputs[EOC_OUTPUT].setVoltage(pulse ? 10 : 0);

}

struct OUAIVEDisplay : OpaqueWidget {
	OUAIVE *module;
	const float width = 125.0f;
	const float height = 50.0f;
	float zoomWidth = 125.0f;
	float zoomLeftAnchor = 0.0f;
	int refIdx = 0;
	float refX = 0.0f;

	OUAIVEDisplay() {

	}

  void onDragStart(const event::DragStart &e) override {
		APP->window->cursorLock();
		OpaqueWidget::onDragStart(e);
	}

  void onDragMove(const event::DragMove &e) override {
		float zoom = 1.0f;
    if (e.mouseDelta.y > 0.0f) {
      zoom = 1.0f/(((APP->window->getMods() & RACK_MOD_MASK) == (GLFW_MOD_SHIFT)) ? 2.0f : 1.1f);
    }
    else if (e.mouseDelta.y < 0.0f) {
      zoom = ((APP->window->getMods() & RACK_MOD_MASK) == (GLFW_MOD_SHIFT)) ? 2.0f : 1.1f;
    }
    zoomWidth = clamp(zoomWidth*zoom,width,zoomWidth*((APP->window->getMods() & RACK_MOD_MASK) == (GLFW_MOD_SHIFT) ? 2.0f : 1.1f));
    zoomLeftAnchor = clamp(refX - (refX - zoomLeftAnchor)*zoom + e.mouseDelta.x, width - zoomWidth,0.0f);
		OpaqueWidget::onDragMove(e);
	}

  void onDragEnd(const event::DragEnd &e) override {
    APP->window->cursorUnlock();
    OpaqueWidget::onDragEnd(e);
  }

	void drawLayer(const DrawArgs& args, int layer) override {
		if (layer == 1) {
			if (module && module->playBuffer.size() > 0) {
				// Copy buffer outside of lock to minimize lock time
				module->lock();
				size_t bufferSize = std::min(size_t(module->totalSampleCount), module->playBuffer.size());
				std::vector<float> vL(bufferSize);
				std::vector<float> vR(bufferSize);
				
				// Batch copy the data while holding the lock
				for (size_t i = 0; i < bufferSize; i++) {
					vL[i] = module->playBuffer[i].samples[0];
					if (module->channels > 1) {
						vR[i] = module->playBuffer[i].samples[1];
					} else {
						vR[i] = module->playBuffer[i].samples[0]; // Copy mono to both channels
					}
				}
				module->unlock();
				
				nvgFontSize(args.vg, 14);
				nvgFillColor(args.vg, YELLOW_BIDOO);

				std::string trigMode = "";
				std::string slices = "";
				if (module->trigMode == 0) {
					trigMode = "TRIG ";
				}
				else if (module->trigMode==1)	{
					trigMode = "GATE ";
				}
				else {
					trigMode = "SLICE ";
					slices = "|" + to_string(module->nbSlices) + "|";
				}

				nvgTextBox(args.vg, 3, -15, 40, trigMode.c_str(), NULL);
				nvgTextBox(args.vg, 59, -15, 40, slices.c_str(), NULL);

				std::string readMode = "";
				if (module->readMode == 0) {
					readMode = ">";
				}
				else if (module->readMode == 2) {
					readMode = ">>";
				}
				else {
					readMode = "<";
				}

				nvgTextBox(args.vg, 40, -15, 40, readMode.c_str(), NULL);

				std::string speed = std::to_string(module->speed);

				nvgTextBox(args.vg, 90, -15, 40, speed.c_str(), NULL);

				//Draw play line
				if ((module->play) && (bufferSize>0)) {
					nvgStrokeColor(args.vg, LIGHTBLUE_BIDOO);
					{
						nvgBeginPath(args.vg);
						nvgStrokeWidth(args.vg, 2);
						if (module->totalSampleCount>0) {
							nvgMoveTo(args.vg, module->samplePos * zoomWidth / bufferSize + zoomLeftAnchor, 0);
							nvgLineTo(args.vg, module->samplePos * zoomWidth / bufferSize + zoomLeftAnchor, 2 * height+10);
						}
						else {
							nvgMoveTo(args.vg, 0, 0);
							nvgLineTo(args.vg, 0, 2 * height+10);
						}
						nvgClosePath(args.vg);
					}
					nvgStroke(args.vg);
				}

				//Draw ref line
				nvgStrokeColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0x30));
				nvgStrokeWidth(args.vg, 1);
				{
					nvgBeginPath(args.vg);
					nvgMoveTo(args.vg, 0, height * 0.5f);
					nvgLineTo(args.vg, width, height * 0.5f);
					nvgClosePath(args.vg);
				}
				nvgStroke(args.vg);

				nvgStrokeColor(args.vg, nvgRGBA(0xff, 0xff, 0xff, 0x30));
				nvgStrokeWidth(args.vg, 1);
				{
					nvgBeginPath(args.vg);
					nvgMoveTo(args.vg, 0, 3*height * 0.5f + 10);
					nvgLineTo(args.vg, width, 3*height * 0.5f + 10);
					nvgClosePath(args.vg);
				}
				nvgStroke(args.vg);

				if ((!module->loading) && (bufferSize>0)) {
					//Draw waveform
					nvgStrokeColor(args.vg, PINK_BIDOO);
					nvgSave(args.vg);
					Rect b = Rect(Vec(zoomLeftAnchor, 0), Vec(zoomWidth, height));
					size_t inc = std::max(bufferSize/zoomWidth/4,1.f);
					nvgScissor(args.vg, 0, b.pos.y, width, height);
					nvgBeginPath(args.vg);
					for (size_t i = 0; i < bufferSize; i+=inc) {
						float x, y;
						x = (float)i/bufferSize;
						y = (-1.f)*vL[i] / 2.0f + 0.5f;
						Vec p;
						p.x = b.pos.x + b.size.x * x;
						p.y = b.pos.y + b.size.y * (1.0f - y);
						if (i == 0) {
							nvgMoveTo(args.vg, p.x, p.y);
						}
						else {
							nvgLineTo(args.vg, p.x, p.y);
						}
					}
					nvgLineCap(args.vg, NVG_MITER);
					nvgStrokeWidth(args.vg, 1);
					nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
					nvgStroke(args.vg);

					b = Rect(Vec(zoomLeftAnchor, height+10), Vec(zoomWidth, height));
					nvgScissor(args.vg, 0, b.pos.y, width, height);
					nvgBeginPath(args.vg);
					for (size_t i = 0; i < bufferSize; i+=inc) {
						float x, y;
						x = (float)i/bufferSize;
						y = (-1.f)*vR[i] / 2.0f + 0.5f;
						Vec p;
						p.x = b.pos.x + b.size.x * x;
						p.y = b.pos.y + b.size.y * (1.0f - y);
						if (i == 0)
							nvgMoveTo(args.vg, p.x, p.y);
						else
							nvgLineTo(args.vg, p.x, p.y);
					}
					nvgLineCap(args.vg, NVG_MITER);
					nvgStrokeWidth(args.vg, 1);
					nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
					nvgStroke(args.vg);
					nvgResetScissor(args.vg);

					//draw slices

					if (module->trigMode == 2) {
						nvgScissor(args.vg, 0, 0, width, 2*height+10);
						for (int i = 1; i < module->nbSlices; i++) {
							nvgStrokeColor(args.vg, YELLOW_BIDOO);
							nvgStrokeWidth(args.vg, 1);
							{
								nvgBeginPath(args.vg);
								nvgMoveTo(args.vg, (int)(i * module->sliceLength * zoomWidth / bufferSize + zoomLeftAnchor) , 0);
								nvgLineTo(args.vg, (int)(i * module->sliceLength * zoomWidth / bufferSize + zoomLeftAnchor) , 2*height+10);
								nvgClosePath(args.vg);
							}
							nvgStroke(args.vg);
						}
						nvgResetScissor(args.vg);
					}

					nvgRestore(args.vg);
				}
			}
		}
		Widget::drawLayer(args, layer);
	}

};

struct OUAIVEWidget : BidooWidget {
	OUAIVEWidget(OUAIVE *module) {
		setModule(module);
		prepareThemes(asset::plugin(pluginInstance, "res/OUAIVE.svg"));

		addChild(createWidget<ScrewSilver>(Vec(15, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 0)));
		addChild(createWidget<ScrewSilver>(Vec(15, 365)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x-30, 365)));

		{
			OUAIVEDisplay *display = new OUAIVEDisplay();
			display->module = module;
			display->box.pos = Vec(5, 70);
			display->box.size = Vec(125, 110);
			addChild(display);
		}

		static const float portX0[4] = {34, 67, 101};

		addInput(createInput<TinyPJ301MPort>(Vec(10, 18), module, OUAIVE::POS_RESET_INPUT));
		addOutput(createOutput<TinyPJ301MPort>(Vec(112, 18), module, OUAIVE::EOC_OUTPUT));

		addParam(createParam<BlueCKD6>(Vec(portX0[0]-25, 215), module, OUAIVE::TRIG_MODE_PARAM));

		addParam(createParam<BlueCKD6>(Vec(portX0[1]-14, 215), module, OUAIVE::READ_MODE_PARAM));
		addInput(createInput<TinyPJ301MPort>(Vec(portX0[2]+5, 222), module, OUAIVE::READ_MODE_INPUT));

		addParam(createParam<BidooBlueTrimpot>(Vec(portX0[1]-9, 250), module, OUAIVE::NB_SLICES_PARAM));
		addParam(createParam<BidooBlueTrimpot>(Vec(portX0[1]+15, 250), module, OUAIVE::CVSLICES_PARAM));
		addInput(createInput<TinyPJ301MPort>(Vec(portX0[2]+5, 252), module, OUAIVE::NB_SLICES_INPUT));

		addParam(createParam<BidooBlueTrimpot>(Vec(portX0[1]-9, 275), module, OUAIVE::SPEED_PARAM));
		addParam(createParam<BidooBlueTrimpot>(Vec(portX0[1]+15, 275), module, OUAIVE::CVSPEED_PARAM));
		addInput(createInput<TinyPJ301MPort>(Vec(portX0[2]+5, 277), module, OUAIVE::SPEED_INPUT));

		addInput(createInput<PJ301MPort>(Vec(7, 330), module, OUAIVE::GATE_INPUT));
		addInput(createInput<PJ301MPort>(Vec(40, 330), module, OUAIVE::POS_INPUT));
		addOutput(createOutput<TinyPJ301MPort>(Vec(portX0[2]-11, 340), module, OUAIVE::OUTL_OUTPUT));
		addOutput(createOutput<TinyPJ301MPort>(Vec(portX0[2]+11, 340), module, OUAIVE::OUTR_OUTPUT));
	}

  struct OUAIVEItem : MenuItem {
  	OUAIVE *module;
  	void onAction(const event::Action &e) override {

  		std::string dir = module->lastPath.empty() ? asset::user("") : rack::system::getDirectory(module->lastPath);
#if defined(METAMODULE)
		async_osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, NULL, [this](char *path) {
			if (path) {
				module->samplePos = 0;
				module->lastPath = path;
				module->sliceIndex = -1;
				module->loading = true;
				free(path);
			}
		});
#else
		char *path = osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, NULL);
		if (path) {
			module->samplePos = 0;
			module->lastPath = path;
			module->sliceIndex = -1;
			module->loading = true;
			free(path);
		}
#endif
  	}
  };

  void appendContextMenu(ui::Menu *menu) override {
		BidooWidget::appendContextMenu(menu);
		OUAIVE *module = dynamic_cast<OUAIVE*>(this->module);
		assert(module);

		menu->addChild(new MenuSeparator());
		menu->addChild(construct<OUAIVEItem>(&MenuItem::text, "Load sample", &OUAIVEItem::module, module));
	}

	void onPathDrop(const PathDropEvent& e) override {
		Widget::onPathDrop(e);
		OUAIVE *module = dynamic_cast<OUAIVE*>(this->module);
		module->samplePos = 0;
		module->lastPath = e.paths[0];
		module->sliceIndex = -1;
		module->loading=true;
	}
};

Model *modelOUAIVE = createModel<OUAIVE, OUAIVEWidget>("OUAIve");
