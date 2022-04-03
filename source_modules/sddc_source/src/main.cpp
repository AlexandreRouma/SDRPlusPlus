#include <imgui.h>
#include <spdlog/spdlog.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <core.h>
#include <thread>
#include <signal_path/signal_path.h>
#include <vector>
#include <gui/tuner.h>
#include <gui/file_dialogs.h>
#include <utils/freq_formatting.h>
#include <gui/dialogs/dialog_box.h>
#include <gui/smgui.h>

#include "resource.h"
#include "Core/RadioHandler.h"
#include "Core/FX3class.h"

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "sddc_source",
    /* Description:     */ "SDDC source module for SDR++",
    /* Author:          */ "Ryzerth;pkuznetsov",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

// firmware data
unsigned char* res_data;
uint32_t res_size;
fx3class* Fx3 = nullptr;

class SDDCSourceModule : public ModuleManager::Instance {
public:
    SDDCSourceModule(std::string name) {
        this->name = name;

        if (core::args["server"].b()) { return; }

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        for (int srate_idx = 0;; srate_idx++) {
            double srate = getRatebyId(srate_idx);

            if (srate < 0)
                break;

            this->sampleRateList.push_back(srate);
            this->sampleRateListTxt += getSampleRate(srate);
            this->sampleRateListTxt += '\0';
        }

        refresh();

        config.acquire();
        if (!config.conf["device"].is_string()) {
            selectedDevName = "";
            config.conf["device"] = "";
        }
        else {
            selectedDevName = config.conf["device"];
        }
        config.release(true);
        selectByName(selectedDevName);

        sigpath::sourceManager.registerSource("SDDC", &handler);
    }

    ~SDDCSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("SDDC");
    }

    void postInit() {
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

    void refresh() {
        unsigned char idx = 0;
        int selected = 0;
        char name[256];

        while (Fx3->Enumerate(idx, name, res_data, res_size) && (idx < MAXNDEV)) {
            // https://en.wikipedia.org/wiki/West_Bridge
            int retry = 2;
            while ((strncmp("WestBridge", name, sizeof("WestBridge")) != NULL) && retry-- > 0)
                Fx3->Enumerate(idx, name, res_data, res_size); // if it enumerates as BootLoader retry

            devNames.push_back(name);
            devListTxt += name;
            devListTxt += '\0';

            idx++;
        }
        devCount = idx;
    }

    void selectFirst() {
        if (devCount != 0) {
            selectById(0);
        }
    }

    void selectByName(std::string name) {
        for (int i = 0; i < devCount; i++) {
            if (devNames[i] == name) {
                selectById(i);
                return;
            }
        }

        selectFirst();
    }

    double getRatebyId(int id)
    {
        double div = pow(2.0, id);
        double srateM = div * 2.0;
        double bwmin = adcnominalfreq / 64.0;
        if (adcnominalfreq > N2_BANDSWITCH) bwmin /= 2.0;
        double srate = bwmin * srateM;

        if ((srate / adcnominalfreq) * 2.0 > 1.1)
            return -1;

        return srate;
    }

    void selectById(int id) {
        unsigned char idx;
        if (id < 0 || id >= devCount) {
            selectedDevName = "";
            return;
        }

        devId = id;
        selectedDevName = devNames[id];

        idx = id;
        char buf[256];
        Fx3->Enumerate(idx, buf, res_data, res_size);

        // Fx3->Enum already specify the device index
        Fx3->Open(res_data, res_size);
        RadioHandler.Init(Fx3, Callback, nullptr, this);

        config.acquire();
        config.conf["device"] = selectedDevName;
        config.release(true);
    
        // check if contains the index section
        bool created = false;
        config.acquire();
        if (!config.conf["devices"].contains(selectedDevName)) {
            created = true;
            config.conf["devices"][selectedDevName]["sampleRateIndex"] = 1;
            config.conf["devices"][selectedDevName]["rfmode"] = HFMODE;
            config.conf["devices"][selectedDevName]["rand"] = RadioHandler.GetDither();
            config.conf["devices"][selectedDevName]["pga"] = RadioHandler.GetPga();
            config.conf["devices"][selectedDevName]["dither"] = RadioHandler.GetDither();
            config.conf["devices"][selectedDevName]["modes"] = json({});
        }

        if (config.conf["devices"][selectedDevName].contains("sampleRateIndex")) {
            srId = config.conf["devices"][selectedDevName]["sampleRateIndex"];
        }

        if (config.conf["devices"][selectedDevName].contains("rfmode")) {
            rfmode = config.conf["devices"][selectedDevName]["rfmode"];
        }

        if (config.conf["devices"][selectedDevName].contains("rand")) {
            rand = config.conf["devices"][selectedDevName]["rand"];
        }

        if (config.conf["devices"][selectedDevName].contains("pga")) {
            pga = config.conf["devices"][selectedDevName]["pga"];
        }

        if (config.conf["devices"][selectedDevName].contains("dither")) {
            dither = config.conf["devices"][selectedDevName]["dither"];
        }

        config.release(created);

        RadioHandler.UptRand(rand);
        RadioHandler.UptPga(pga);
        RadioHandler.UptDither(dither);

        switch_mode(rfmode);
    }

private:
    static std::string getBandwdithScaled(double bw) {
        char buf[1024];
        if (bw >= 1000000.0) {
            sprintf(buf, "%.1lfMHz", bw / 1000000.0);
        }
        else if (bw >= 1000.0) {
            sprintf(buf, "%.1lfKHz", bw / 1000.0);
        }
        else {
            sprintf(buf, "%.1lfHz", bw);
        }
        return std::string(buf);
    }

    static std::string getSampleRate(double rate) {
        char buf[1024];
        sprintf(buf, "%.1lf MHz", rate / 1000000);
        return std::string(buf);
    }

    static void Callback(void* ctx, const float* data, uint32_t len) {
        SDDCSourceModule* _this = (SDDCSourceModule*)ctx;

        if (data) {
            memcpy(_this->stream.writeBuf, data, len * sizeof(float) * 2);
            _this->stream.swap(len);
        }
    }

    static void menuSelected(void* ctx) {
        SDDCSourceModule* _this = (SDDCSourceModule*)ctx;

        spdlog::info("SDDCSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        SDDCSourceModule* _this = (SDDCSourceModule*)ctx;

        spdlog::info("SDDCSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        SDDCSourceModule* _this = (SDDCSourceModule*)ctx;
        if (_this->running) { return; }
        if (_this->selectedDevName == "") { return; }

        // Start device
        _this->RadioHandler.Start(_this->srId);
        _this->RadioHandler.TuneLO(_this->freq);

        if (_this->RadioHandler.IsReady()) //  HF103 connected
        {
            _this->running = true;
        }

        spdlog::info("SDDCSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        SDDCSourceModule* _this = (SDDCSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        _this->stream.stopWriter();

        // Stop device
        _this->RadioHandler.Stop();

        _this->stream.clearWriteStop();
        spdlog::info("SDDCSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        SDDCSourceModule* _this = (SDDCSourceModule*)ctx;
        if (_this->running) {
            _this->RadioHandler.TuneLO(freq);
        }
        _this->freq = freq;
        spdlog::info("SDDCSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        SDDCSourceModule* _this = (SDDCSourceModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvail().x;

        if (_this->running) { style::beginDisabled(); }

        SmGui::SetNextItemWidth(menuWidth);
        if (SmGui::Combo(CONCAT("##_sddc_dev_sel_", _this->name), &_this->devId, _this->devListTxt.c_str())) {
            _this->selectById(_this->devId);
        }

        SmGui::LeftLabel("Mode");
        SmGui::FillWidth();
        int mode = _this->rfmode == HFMODE ? 0 : 1;
        if (SmGui::Combo(CONCAT("##_sddc_rfmode_sel_", _this->name), &mode, "HF\0VHF\0")) {
            _this->rfmode = mode == 0 ? HFMODE : VHFMODE;
            _this->switch_mode(_this->rfmode);
            if (_this->selectedDevName != "") {
                config.acquire();
                config.conf["devices"][_this->selectedDevName]["rfmode"] = _this->rfmode;
                config.release(true);
            }
        }

        if (SmGui::Combo(CONCAT("##_sddc_sr_sel_", _this->name), &_this->srId, _this->sampleRateListTxt.c_str())) {
            core::setInputSampleRate(_this->sampleRateList[_this->srId]);
            if (_this->selectedDevName != "") {
                config.acquire();
                config.conf["devices"][_this->selectedDevName]["sampleRateIndex"] = _this->srId;
                config.release(true);
            }
        }

        SmGui::SameLine();
        float refreshBtnWdith = menuWidth - ImGui::GetCursorPosX();
        if (SmGui::Button(CONCAT("Refresh##_sddc_refr_", _this->name), ImVec2(refreshBtnWdith, 0))) {
            _this->refresh();
            // Reselect and reset samplerate if it changed
        }

        if (_this->running) { style::endDisabled(); }


        if (SmGui::Checkbox(CONCAT("RAND##_sddc_rand_", _this->name), &_this->rand)) {
            _this->RadioHandler.UptRand(_this->rand);
            if (_this->selectedDevName != "") {
                config.acquire();
                config.conf["devices"][_this->selectedDevName]["rand"] = _this->rand;
                config.release(true);
            }
        }

        SmGui::SameLine();
        if (SmGui::Checkbox(CONCAT("PGA##_sddc_pga_", _this->name), &_this->pga)) {
            _this->RadioHandler.UptPga(_this->pga);
            if (_this->selectedDevName != "") {
                config.acquire();
                config.conf["devices"][_this->selectedDevName]["pga"] = _this->pga;
                config.release(true);
            }
        }

        SmGui::SameLine();
        if (SmGui::Checkbox(CONCAT("Dither##_sddc_dither_", _this->name), &_this->dither)) {
            _this->RadioHandler.UptDither(_this->dither);
            if (_this->selectedDevName != "") {
                config.acquire();
                config.conf["devices"][_this->selectedDevName]["dither"] = _this->dither;
                config.release(true);
            }
        }

        // All other controls
        SmGui::LeftLabel("RF");
        SmGui::FillWidth();
        if (SmGui::SliderFloatWithSteps(CONCAT("##_sddc_rf_sel_", _this->name), &_this->rf_steps_select, _this->rf_steps[0], _this->rf_steps[_this->rf_steps_length - 1], 1, SmGui::FMT_STR_FLOAT_DB_NO_DECIMAL)) {
            _this->set_rf_steps();
            if (_this->selectedDevName != "") {
                config.acquire();
                config.conf["devices"][_this->selectedDevName]["modes"][_this->mode_str]["rf_step"] = _this->rf_steps_select;
                config.release(true);
            }
        }

        SmGui::LeftLabel("IF");
        SmGui::FillWidth();
        if (SmGui::SliderFloatWithSteps(CONCAT("##_sddc_if_sel_", _this->name), &_this->if_steps_select, _this->if_steps[0], _this->if_steps[_this->if_steps_length - 1], 1, SmGui::FMT_STR_FLOAT_DB_NO_DECIMAL)) {
            _this->set_if_steps();
            if (_this->selectedDevName != "") {
                config.acquire();
                config.conf["devices"][_this->selectedDevName]["modes"][_this->mode_str]["if_step"] = _this->if_steps_select;
                config.release(true);
            }
        }

        if (SmGui::Checkbox(CONCAT("biasT##_sddc_bias_", _this->name), &_this->biasT)) {
            _this->set_biasT();
            if (_this->selectedDevName != "") {
                config.acquire();
                config.conf["devices"][_this->selectedDevName]["modes"][_this->mode_str]["biasT"] = _this->biasT;
                config.release(true);
            }
        }
    }

    void set_if_steps() {
        for (int i = 0; i < if_steps_length - 1; i++) {
            if (if_steps_select < if_steps[i + 1]) {
                RadioHandler.UpdateIFGain(i);
                return;
            }
        }
    }

    void set_rf_steps() {
        for (int i = 0; i < rf_steps_length - 1; i++) {
            if (rf_steps_select < rf_steps[i + 1]) {
                RadioHandler.UpdateattRF(i);
                return;
            }
        }
    }

    void set_biasT() {
        if (rfmode == HFMODE)
            RadioHandler.UpdBiasT_HF(biasT);
        else if (rfmode == VHFMODE)
            RadioHandler.UpdBiasT_VHF(biasT);
    }

    void switch_mode(rf_mode mode) {
        mode_str = (mode == HFMODE) ? "HF" : "VHF";

        RadioHandler.UpdatemodeRF(mode);
        rf_steps_length = RadioHandler.GetRFAttSteps(&rf_steps);

        if_steps_length = RadioHandler.GetIFGainSteps(&if_steps);

        bool created = false;
        config.acquire();
        if (!config.conf["devices"][selectedDevName]["modes"].contains(mode_str)) {
            created = true;
            config.conf["devices"][selectedDevName]["modes"][mode_str]["biasT"] = false;
            config.conf["devices"][selectedDevName]["modes"][mode_str]["rf_step"] = 0;
            config.conf["devices"][selectedDevName]["modes"][mode_str]["if_step"] = 0;
        }

        if (config.conf["devices"][selectedDevName]["modes"][mode_str].contains("rf_step")) {
            rf_steps_select = config.conf["devices"][selectedDevName]["modes"][mode_str]["rf_step"];
        }

        if (config.conf["devices"][selectedDevName]["modes"][mode_str].contains("if_step")) {
            if_steps_select = config.conf["devices"][selectedDevName]["modes"][mode_str]["if_step"];
        }

        if (config.conf["devices"][selectedDevName]["modes"][mode_str].contains("biasT")) {
            biasT = config.conf["devices"][selectedDevName]["modes"][mode_str]["biasT"];
        }

        config.release(created);

        set_biasT();
        set_if_steps();
        set_rf_steps();
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;
    bool running = false;

    double freq;

    int devCount = 0;
    std::vector<std::string> devNames;
    std::string selectedDevName = "";
    std::string devListTxt;
    int devId = 0;

    std::vector<uint32_t> sampleRateList;
    std::string sampleRateListTxt;
    int srId;

    bool rand;
    bool pga;
    bool dither;

    // settings
    std::string mode_str;
    rf_mode rfmode;

    // mode specific settings
    const float* if_steps;
    int if_steps_length;
    float if_steps_select;

    const float* rf_steps;
    int rf_steps_length;
    float rf_steps_select;

    bool biasT;

    RadioHandlerClass RadioHandler;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    Fx3 = CreateUsbHandler();

#if (_WIN32)
    // Load firmware
    HMODULE hInst = NULL;
    GetModuleHandleEx(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        (LPCTSTR)_INIT_,
        &hInst);

    HRSRC res = FindResource(hInst, MAKEINTRESOURCE(RES_BIN_FIRMWARE), RT_RCDATA);
    HGLOBAL res_handle = LoadResource(hInst, res);
    res_data = (unsigned char*)LockResource(res_handle);
    res_size = SizeofResource(hInst, res);
#else
    FILE* fp = fopen((core::args["root"].s() + "/SDDC_FX3.img").c_str(), "rb");
    if (fp != nullptr) {
        fseek(fp, 0, SEEK_END);
        res_size = ftell(fp);
        res_data = (unsigned char*)malloc(res_size);
        fseek(fp, 0, SEEK_SET);
        fread(res_data, 1, res_size, fp);
    }
#endif

    def["devices"] = json({});
    def["device"] = "";
    config.setPath(core::args["root"].s() + "/sddc_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT void* _CREATE_INSTANCE_(std::string name) {
    return new SDDCSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (SDDCSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    delete Fx3;
    config.disableAutoSave();
    config.save();
}