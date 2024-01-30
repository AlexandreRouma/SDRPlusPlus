#include <config.h>
#include <core.h>
#include <filesystem>
#include <gui/gui.h>
#include <gui/style.h>
#include <gui/widgets/image.h>
#include <imgui.h>
#include <module.h>
#include <signal_path/signal_path.h>

#include <dsp/demod/quadrature.h>
#include <dsp/sink/handler_sink.h>
#include "linesync.h"
#include <dsp/loop/pll.h>
#include <dsp/convert/real_to_complex.h>
#include <dsp/filter/fir.h>
#include <dsp/taps/from_array.h>

#include "chrominance_filter.h"

#include "chroma_pll.h"

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{/* Name:            */ "atv_decoder",
               /* Description:     */ "ATV decoder for SDR++",
               /* Author:          */ "Ryzerth",
               /* Version:         */ 0, 1, 0,
               /* Max instances    */ -1
};

#define SAMPLE_RATE (625.0f * 720.0f * 25.0f)

class ATVDecoderModule : public ModuleManager::Instance {
  public:
    ATVDecoderModule(std::string name) : img(720, 625) {
        this->name = name;

        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, 8000000.0f, SAMPLE_RATE, SAMPLE_RATE, SAMPLE_RATE, true);

        demod.init(vfo->output, SAMPLE_RATE, SAMPLE_RATE / 2.0f);
        sync.init(&demod.out, 1.0f, 1e-6, 1.0, 0.05);
        sink.init(&sync.out, handler, this);

        r2c.init(NULL);
        chromaTaps = dsp::taps::fromArray(CHROMA_FIR_SIZE, CHROMA_FIR);
        fir.init(NULL, chromaTaps);
        pll.init(NULL, 0.01, 0.0, dsp::math::hzToRads(4433618.75, SAMPLE_RATE), dsp::math::hzToRads(4433618.75*0.90, SAMPLE_RATE), dsp::math::hzToRads(4433618.75*1.1, SAMPLE_RATE));

        demod.start();
        sync.start();
        sink.start();

        gui::menu.registerEntry(name, menuHandler, this, this);
    }

    ~ATVDecoderModule() {
        if (vfo) {
            sigpath::vfoManager.deleteVFO(vfo);
        }
        demod.stop();
        gui::menu.removeEntry(name);
    }

    void postInit() {}

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() { return enabled; }

  private:
    static void menuHandler(void *ctx) {
        ATVDecoderModule *_this = (ATVDecoderModule *)ctx;

        if (!_this->enabled) {
            style::beginDisabled();
        }

        // Ideal width for testing: 750pixels

        ImGui::FillWidth();
        _this->img.draw();

        ImGui::LeftLabel("Sync");
        ImGui::FillWidth();
        ImGui::SliderFloat("##syncLvl", &_this->sync_level, -2, 2);

        ImGui::LeftLabel("Min");
        ImGui::FillWidth();
        ImGui::SliderFloat("##minLvl", &_this->minLvl, -1.0, 1.0);

        ImGui::LeftLabel("Span");
        ImGui::FillWidth();
        ImGui::SliderFloat("##spanLvl", &_this->spanLvl, 0, 1.0);

        ImGui::LeftLabel("Sync Bias");
        ImGui::FillWidth();
        ImGui::SliderFloat("##syncBias", &_this->sync.syncBias,-0.1, 0.1);

        if (ImGui::Button("Test2")) {
            _this->sync.test2 = true;
        }

        if (ImGui::Button("Switch frame")) {
            std::lock_guard<std::mutex> lck(_this->evenFrameMtx);
            _this->evenFrame = !_this->evenFrame;
        }

        if (_this->sync.locked) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Locked");
        }
        else {
            ImGui::TextUnformatted("Not locked");
        }

        ImGui::Checkbox("Force Lock", &_this->sync.forceLock);

        if (!_this->enabled) {
            style::endDisabled();
        }
    }

    static void handler(float *data, int count, void *ctx) {
        ATVDecoderModule *_this = (ATVDecoderModule *)ctx;

        // Convert line to complex
        _this->r2c.process(720, data, _this->r2c.out.writeBuf);

        // Isolate the chroma subcarrier
        _this->fir.process(720, _this->r2c.out.writeBuf, _this->fir.out.writeBuf);

        // Run chroma carrier through the PLL
        _this->pll.process(720, _this->fir.out.writeBuf, _this->pll.out.writeBuf, ((_this->ypos%2)==1) ^ _this->evenFrame);

        // Render line to the image without color
        //int lypos = _this->ypos - 1;
        //if (lypos < 0) { lypos = 624; }
        //uint32_t* lastLine = &((uint32_t *)_this->img.buffer)[(lypos < 313) ? (lypos*720*2) : ((((lypos - 313)*2)+1)*720) ];
        //uint32_t* currentLine = &((uint32_t *)_this->img.buffer)[(_this->ypos < 313) ? (_this->ypos*720*2) : ((((_this->ypos - 313)*2)+1)*720) ];

        uint32_t* currentLine = &((uint32_t *)_this->img.buffer)[_this->ypos*720];

        for (int i = 0; i < count; i++) {
            //float imval = std::clamp<float>((data[i] - _this->minLvl) * 255.0 / _this->spanLvl, 0, 255);
            uint32_t re = std::clamp<float>((_this->pll.out.writeBuf[i].re - _this->minLvl) * 255.0 / _this->spanLvl, 0, 255);
            uint32_t im = std::clamp<float>((_this->pll.out.writeBuf[i].im - _this->minLvl) * 255.0 / _this->spanLvl, 0, 255);
            currentLine[i] = 0xFF000000 | (im << 8) | re;
        }
    
        // Vertical scan logic
        _this->ypos++;
        bool rollover = _this->ypos >= 625;
        if (rollover) {
            {
                std::lock_guard<std::mutex> lck(_this->evenFrameMtx);
                _this->evenFrame = !_this->evenFrame;
            }
            _this->ypos = 0;
            _this->img.swap();
        }

        // Measure vsync levels
        float sync0 = 0.0f, sync1 = 0.0f;
        for (int i = 0; i < 306; i++) {
            sync0 += data[i];
        }
        for (int i = (720/2); i < ((720/2)+306); i++) {
            sync1 += data[i];
        }
        sync0 *= (1.0f/305.0f);
        sync1 *= (1.0f/305.0f);

        // Save sync detection to history
        _this->syncHistory >>= 2;
        _this->syncHistory |= (((uint16_t)(sync1 < _this->sync_level)) << 9) | (((uint16_t)(sync0 < _this->sync_level)) << 8);

        // Trigger vsync in case one is detected
        // TODO: Also sync with odd field
        if (!rollover && _this->syncHistory == 0b0000011111) {
            {
                std::lock_guard<std::mutex> lck(_this->evenFrameMtx);
                _this->evenFrame = !_this->evenFrame;
            }
            _this->ypos = 0;
            _this->img.swap();
        }
    }

    std::string name;
    bool enabled = true;

    VFOManager::VFO *vfo = NULL;
    dsp::demod::Quadrature demod;
    LineSync sync;
    dsp::sink::Handler<float> sink;
    dsp::convert::RealToComplex r2c;
    dsp::tap<dsp::complex_t> chromaTaps;
    dsp::filter::FIR<dsp::complex_t, dsp::complex_t> fir;
    dsp::loop::ChromaPLL pll;
    int ypos = 0;

    bool evenFrame = false;
    std::mutex evenFrameMtx;

    float sync_level = -0.06f;
    int sync_count = 0;
    int short_sync = 0;

    float minLvl = 0.0f;
    float spanLvl = 1.0f;

    bool lockedLines = 0;
    uint16_t syncHistory = 0;

    ImGui::ImageDisplay img;
};

MOD_EXPORT void _INIT_() {}

MOD_EXPORT ModuleManager::Instance *_CREATE_INSTANCE_(std::string name) { return new ATVDecoderModule(name); }

MOD_EXPORT void _DELETE_INSTANCE_(void *instance) { delete (ATVDecoderModule *)instance; }

MOD_EXPORT void _END_() {}