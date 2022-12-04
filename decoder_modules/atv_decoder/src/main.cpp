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

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{/* Name:            */ "atv_decoder",
               /* Description:     */ "ATV decoder for SDR++",
               /* Author:          */ "Ryzerth",
               /* Version:         */ 0, 1, 0,
               /* Max instances    */ -1};

#define SAMPLE_RATE (625.0f * 720.0f * 25.0f)

class ATVDecoderModule : public ModuleManager::Instance {
  public:
    ATVDecoderModule(std::string name) : img(720, 625) {
        this->name = name;

        vfo = sigpath::vfoManager.createVFO(name, ImGui::WaterfallVFO::REF_CENTER, 0, 8000000.0f, SAMPLE_RATE, SAMPLE_RATE, SAMPLE_RATE, true);

        demod.init(vfo->output, SAMPLE_RATE, SAMPLE_RATE / 2.0f);
        sink.init(&demod.out, handler, this);

        demod.start();
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

    void enable() { enabled = true; }

    void disable() { enabled = false; }

    bool isEnabled() { return enabled; }

  private:
    static void menuHandler(void *ctx) {
        ATVDecoderModule *_this = (ATVDecoderModule *)ctx;

        if (!_this->enabled) {
            style::beginDisabled();
        }

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

        if (!_this->enabled) {
            style::endDisabled();
        }
    }

    static void handler(float *data, int count, void *ctx) {
        ATVDecoderModule *_this = (ATVDecoderModule *)ctx;

        uint8_t *buf = (uint8_t *)_this->img.buffer;
        float val;
        float imval;
        int pos = 0;
        for (int i = 0; i < count; i++) {
            val = data[i];
            // Sync
            if (val < _this->sync_level) {
                _this->sync_count++;
            }
            else {
                if (_this->sync_count >= 300) {
                    _this->short_sync = 0;
                }
                else if (_this->sync_count >= 33) {
                    if (_this->short_sync == 5) {
                        _this->even_field = false;
                        _this->ypos = 0;
                        _this->img.swap();
                        buf = (uint8_t *)_this->img.buffer;
                    }
                    else if (_this->short_sync == 4) {
                        _this->even_field = true;
                        _this->ypos = 0;
                    }
                    _this->xpos = 0;
                    _this->short_sync = 0;
                }
                else if (_this->sync_count >= 15) {
                    _this->short_sync++;
                }
                _this->sync_count = 0;
            }


            // Draw
            imval = std::clamp<float>((val - _this->minLvl) * 255.0 / _this->spanLvl, 0, 255);
            if (_this->even_field) {
                pos = ((720 * _this->ypos * 2) + _this->xpos) * 4;
            }
            else {
                pos = ((720 * (_this->ypos * 2 + 1)) + _this->xpos) * 4;
            }

            buf[pos] = imval;
            buf[pos + 1] = imval;
            buf[pos + 2] = imval;
            buf[pos + 3] = imval;

            // Image logic
            _this->xpos++;
            if (_this->xpos >= 720) {
                _this->ypos++;
                _this->xpos = 0;
            }
            if (_this->ypos >= 312) {
                _this->ypos = 0;
                _this->xpos = 0;
                _this->even_field = !_this->even_field;
                if (_this->even_field) {
                    _this->img.swap();
                    buf = (uint8_t *)_this->img.buffer;
                }
            }
        }
    }

    std::string name;
    bool enabled = true;

    VFOManager::VFO *vfo = NULL;
    dsp::demod::Quadrature demod;
    dsp::sink::Handler<float> sink;

    int xpos = 0;
    int ypos = 0;
    bool even_field = false;

    float sync_level = -0.3f;
    int sync_count = 0;
    int short_sync = 0;

    float minLvl = 0.0f;
    float spanLvl = 1.0f;

    ImGui::ImageDisplay img;
};

MOD_EXPORT void _INIT_() {}

MOD_EXPORT ModuleManager::Instance *_CREATE_INSTANCE_(std::string name) { return new ATVDecoderModule(name); }

MOD_EXPORT void _DELETE_INSTANCE_(void *instance) { delete (ATVDecoderModule *)instance; }

MOD_EXPORT void _END_() {}