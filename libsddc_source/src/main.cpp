#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <dsp/window.h>
#include <dsp/resampling.h>
#include <dsp/math.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <gui/widgets/combo_as_slider.h>

#include <libsddc.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO {
    /* Name:            */ "libsddc_source",
    /* Description:     */ "BBRF103, HF103, RX888, RX888 mkII source module for SDR++",
    /* Author:          */ "pkuznetsov",
    /* Version:         */ 0, 1, 1,
    /* Max instances    */ 1
};

const int64_t sampleRates[] = {
    2000000,
    4000000,
    8000000,
    16000000,
    32000000,
    64000000
};

const char* sampleRatesTxt[] = {
    "2MHz",
    "4MHz",
    "8MHz",
    "16MHz",
    "32MHz",
    "64MHz"
};

class LibsddcSourceModule : public ModuleManager::Instance {
public:
    LibsddcSourceModule(std::string name) {
        this->name = name;

        core::configManager.aquire();
        std::string image = std::string(core::configManager.conf["resourcesDirectory"])+"/firmware/SDDC_FX3.img";
        core::configManager.release();

        int count = sddc_get_device_count();
        if (count < 0) {
            spdlog::error("LibsddcSourceModule - sddc_get_device_count() failed\n");
            return;
        }
        spdlog::info("LibsddcSourceModule: device count='{0}'\n", count);

        /* get device info */
        struct sddc_device_info *sddc_device_infos;
        count = sddc_get_device_info(&sddc_device_infos);
        if (count < 0) {
            spdlog::error("LibsddcSourceModule - sddc_get_device_info() failed\n");
            return;
        }
        for (int i = 0; i < count; ++i) {
            spdlog::info("LibsddcSourceModule: {0} - manufacturer=\"{1}\" product=\"{2}\" serial number=\"{3}\"\n",
                i, sddc_device_infos[i].manufacturer, sddc_device_infos[i].product,
                sddc_device_infos[i].serial_number);
        }
        sddc_free_device_info(sddc_device_infos);

        /* open and close device */
        sddc = sddc_open(0, image.c_str(), 0);
        if (sddc == 0) {
            spdlog::error("LibsddcSourceModule - sddc_open() failed\n");
            return;
        }

        sampleRateListTxt = "";
        for (int i = 0; i < 5; i++) {
            sampleRateListTxt += sampleRatesTxt[i];
            sampleRateListTxt += '\0';
        }

        sampleRate = sampleRates[srId];
        sddc_set_sample_rate(sddc, sampleRate);


        sddc_set_rf_mode(sddc, vhf ? VHF_MODE : HF_MODE);

        int rf_count = sddc_get_tuner_rf_attenuations(sddc, (const float**) &attensRfArr);
        for (int i = 0; i < rf_count; i++) {
            char temp[32];
            sprintf(temp, "%f", attensRfArr[i]);
            attenRFListTxt += temp;
            attenRFListTxt += '\0';
        }
        attenRf = attensRfArr[0];

        int if_count = sddc_get_tuner_if_attenuations(sddc, (const float**) &attensIfArr);
        for (int i = 0; i < if_count; i++) {
            char temp[32];
            sprintf(temp, "%f", attensIfArr[i]);
            attenIFListTxt += temp;
            attenIFListTxt += '\0';
        }
        attenIf = attensIfArr[0];

        sddc_set_tuner_rf_attenuation(sddc, attenRf);
        sddc_set_tuner_if_attenuation(sddc, attenIf);
        sddc_set_pga(sddc, pga ? 1 : 0);
        sddc_set_adc_rate(sddc, adcOverclock ? 2*64000000 : 64000000);
        adcDither = sddc_get_adc_dither(sddc) != 0;
        adcRandom = sddc_get_adc_random(sddc) != 0;

        sddc_set_hf_bias(sddc, hfBias ? 1 : 0);
        sddc_set_vhf_bias(sddc, vhfBias ? 1 : 0);

        sddc_set_async_params(sddc, 0, 0, asyncHandler, 0, this);

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;
        sigpath::sourceManager.registerSource("LibSDDC", &handler);

        std::string sddcHWModelStr;
        const char* sddcHWModel = sddc_get_hw_model_name(sddc);
        if(sddcHWModel == nullptr) {
            sddcHWModelStr = "Unknown";
        } else {
            sddcHWModelStr = sddcHWModel;
        }
        spdlog::info("LibsddcSourceModule '{0}'-'{1}'-v{2}: Instance created!", name, sddcHWModelStr, sddc_get_firmware(sddc));
    }

    ~LibsddcSourceModule() {
        /* done */
        sddc_close(sddc);
        spdlog::info("LibsddcSourceModule '{0}': Instance deleted!", name);
    }

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

private:

    static void menuSelected(void* ctx) {
        LibsddcSourceModule* _this = (LibsddcSourceModule*)ctx;
        spdlog::info("LibsddcSourceModule '{0}': Menu Select!", _this->name);

        core::setInputSampleRate(_this->sampleRate);
    }

    static void menuDeselected(void* ctx) {
        LibsddcSourceModule* _this = (LibsddcSourceModule*)ctx;
        spdlog::info("LibsddcSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        LibsddcSourceModule* _this = (LibsddcSourceModule*)ctx;

        sddc_set_sample_rate(_this->sddc, _this->sampleRate);
        core::setInputSampleRate(_this->sampleRate);
        sddc_set_tuner_frequency(_this->sddc, _this->freq);

        _this->running = true;
        _this->doStart();

        spdlog::info("LibsddcSourceModule '{0}': Start!", _this->name);
    }

    void doStart() {
        if (sddc_start_streaming(sddc) < 0) {
            spdlog::error("LibsddcSourceModule - sddc_start_streaming() failed\n");
            return;
        }
    }

    static void stop(void* ctx) {
        LibsddcSourceModule* _this = (LibsddcSourceModule*)ctx;
        _this->running = false;
        _this->stream.stopWriter();
        _this->stream.clearWriteStop();
        if (sddc_stop_streaming(_this->sddc) < 0) {
            spdlog::error("LibsddcSourceModule - sddc_stop_streaming() failed\n");
            return;
        }
        spdlog::info("LibsddcSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        LibsddcSourceModule* _this = (LibsddcSourceModule*)ctx;
        _this->freq = freq;
        if (sddc_set_tuner_frequency(_this->sddc, _this->freq) < 0) {
            spdlog::error("LibsddcSourceModule - sddc_set_tuner_frequency() failed\n");
            return;
        }

        spdlog::info("LibsddcSourceModule '{0}': Tune: {1}", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        LibsddcSourceModule* _this = (LibsddcSourceModule*)ctx;
        ImGui::Text("Sample Rate");
        ImGui::SameLine();
        if (ImGui::ComboAsSlider(CONCAT("##_libsddc_sr_sel_", _this->name), &_this->srId, _this->sampleRateListTxt.c_str())) {
            _this->sampleRate = sampleRates[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
            if(_this->running)
                sddc_stop_streaming(_this->sddc);
            sddc_set_sample_rate(_this->sddc, _this->sampleRate);
            if(_this->running)
                 sddc_start_streaming(_this->sddc);
        }
        ImGui::Text("RF");
        ImGui::SameLine();
        if (ImGui::ComboAsSlider(CONCAT("##_libsddc_rf_sel_", _this->name), &_this->attenRfId, _this->attenRFListTxt.c_str())) {
            _this->attenRf = _this->attensRfArr[_this->attenRfId];
            sddc_set_tuner_rf_attenuation(_this->sddc, _this->attenRf);
        }
        if(_this->attenIFListTxt != "\0"){
            ImGui::Text("IF");
            ImGui::SameLine();
            if (ImGui::ComboAsSlider(CONCAT("##_libsddc_if_sel_", _this->name), &_this->attenIfId, _this->attenIFListTxt.c_str())) {
                _this->attenIf = _this->attensIfArr[_this->attenIfId];
                sddc_set_tuner_if_attenuation(_this->sddc, _this->attenIf);
            }
        }
        if(sddc_support_pga(_this->sddc) > 0){
            ImGui::Text("PGA");
            ImGui::SameLine();
            if (ImGui::Checkbox(CONCAT("##_libsddc_pga_", _this->name), &_this->pga)) {
                sddc_set_pga(_this->sddc, _this->pga ? 1 : 0);
            }
        }
        ImGui::Text("VHF");
        ImGui::SameLine();
        if (ImGui::Checkbox(CONCAT("##_libsddc_mode_sel_", _this->name), &_this->vhf)) {
            sddc_set_rf_mode(_this->sddc, _this->vhf ? VHF_MODE : HF_MODE);
            int rf_count = sddc_get_tuner_rf_attenuations(_this->sddc, (const float**) &_this->attensRfArr);
            for (int i = 0; i < rf_count; i++) {
                char temp[32];
                sprintf(temp, "%f", _this->attensRfArr[i]);
                _this->attenRFListTxt += temp;
                _this->attenRFListTxt += '\0';
            }
            _this->attenRfId = 0;
            _this->attenRf = _this->attensRfArr[0];

            int if_count = sddc_get_tuner_if_attenuations(_this->sddc, (const float**) &_this->attensIfArr);
            for (int i = 0; i < if_count; i++) {
                char temp[32];
                sprintf(temp, "%f", _this->attensIfArr[i]);
                _this->attenIFListTxt += temp;
                _this->attenIFListTxt += '\0';
            }
            _this->attenIfId = 0;
            _this->attenIf = _this->attensIfArr[0];

            sddc_set_tuner_rf_attenuation(_this->sddc, _this->attenRf);
            sddc_set_tuner_if_attenuation(_this->sddc, _this->attenIf);
        }
        ImGui::Text("BiasT HF");
        ImGui::SameLine();
        if (ImGui::Checkbox(CONCAT("##_libsddc_bias_hf_", _this->name), &_this->hfBias)) {
            sddc_set_hf_bias(_this->sddc, _this->hfBias ? 1 : 0);
        }
        ImGui::Text("BiasT VHF");
        ImGui::SameLine();
        if (ImGui::Checkbox(CONCAT("##_libsddc_bias_vhf_", _this->name), &_this->vhfBias)) {
            sddc_set_vhf_bias(_this->sddc, _this->vhfBias ? 1 : 0);
        }
        if(sddc_support_128adc(_this->sddc) > 0){
            ImGui::Text("ADC 128M");
            ImGui::SameLine();
            if (ImGui::Checkbox(CONCAT("##_libsddc_adc_128_", _this->name), &_this->adcOverclock)) {
                if(_this->running)
                    sddc_stop_streaming(_this->sddc);
                if(!_this->adcOverclock && _this->srId == 5) {
                    _this->srId = 4;
                    _this->sampleRate = sampleRates[_this->srId];
                }
                sddc_set_sample_rate(_this->sddc, _this->sampleRate);
                core::setInputSampleRate(_this->sampleRate);
                if(_this->running)
                    sddc_start_streaming(_this->sddc);
                _this->sampleRateListTxt = "";
                for (int i = 0; i < (_this->adcOverclock ? 6 : 5); i++) {
                    _this->sampleRateListTxt += sampleRatesTxt[i];
                    _this->sampleRateListTxt += '\0';
                }
            }
        }
        ImGui::Text("ADC Dither");
        ImGui::SameLine();
        if (ImGui::Checkbox(CONCAT("##_libsddc_adcDither_", _this->name), &_this->adcDither)) {
            sddc_set_adc_dither(_this->sddc, _this->adcDither ? 1 : 0);
        }
        ImGui::Text("ADC Random");
        ImGui::SameLine();
        if (ImGui::Checkbox(CONCAT("##_libsddc_adcRandom_", _this->name), &_this->adcRandom)) {
            sddc_set_adc_random(_this->sddc, _this->adcRandom ? 1 : 0);
        }
    }

    static void asyncHandler(uint32_t len, float *buf, void *ctx) {
        LibsddcSourceModule* _this = (LibsddcSourceModule*)ctx;
        memcpy(_this->stream.writeBuf, buf, len*sizeof(float));
        if (!_this->stream.swap(len / 2)) { return; }
    }

    std::string name;
    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;
    double freq = 100e6;
    int64_t sampleRate;
    int srId = 0;
    sddc_t *sddc;
    bool running = false;
    bool enabled = true;
    bool vhf = true;
    float attenRf = 0;
    int attenRfId = 0;
    float attenIf = 0;
    int attenIfId = 0;
    float *attensRfArr;
    float *attensIfArr;
    std::string attenRFListTxt;
    std::string attenIFListTxt;
    std::string sampleRateListTxt;
    bool pga = false;
    bool adcOverclock = false;
    bool hfBias = false;
    bool vhfBias = false;
    bool adcDither = false;
    bool adcRandom = false;
};

MOD_EXPORT void _INIT_() {
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new LibsddcSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (LibsddcSourceModule*)instance;
}
MOD_EXPORT void _END_() {
}
