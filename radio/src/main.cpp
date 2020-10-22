#include <imgui.h>
#include <module.h>
#include <path.h>
#include <watcher.h>
#include <config.h>
#include <core.h>

#define CONCAT(a, b)    ((std::string(a) + b).c_str())
#define DEEMP_LIST      "50µS\00075µS\000none\000"

MOD_INFO {
    /* Name:        */ "radio",
    /* Description: */ "Radio module for SDR++",
    /* Author:      */ "Ryzerth",
    /* Version:     */ "0.2.5"
};

class RadioModule {
public:
    RadioModule(std::string name) {
        this->name = name;
        demod = 1;
        bandWidth = 200000;
        bandWidthMin = 100000;
        bandWidthMax = 200000;
        sigPath.init(name, 200000, 1000);
        sigPath.start();
        sigPath.setDemodulator(SigPath::DEMOD_FM, bandWidth);
        sigPath.vfo->setSnapInterval(100000.0);
        gui::menu.registerEntry(name, menuHandler, this);

        ScriptManager::ScriptRunHandler_t handler;
        handler.ctx = this;
        handler.handler = scriptCreateHandler;
        core::scriptManager.bindScriptRunHandler(name, handler);
    }

    ~RadioModule() {
        // TODO: Implement destructor
    }

private:
    static void menuHandler(void* ctx) {
        RadioModule* _this = (RadioModule*)ctx;
        float menuColumnWidth = ImGui::GetContentRegionAvailWidth();

        ImGui::BeginGroup();

        // TODO: Change VFO ref in signal path

        ImGui::Columns(4, CONCAT("RadioModeColumns##_", _this->name), false);
        if (ImGui::RadioButton(CONCAT("NFM##_", _this->name), _this->demod == 0) && _this->demod != 0) { 
            _this->demod = 0;
            _this->bandWidth = 16000;
            _this->bandWidthMin = 8000;
            _this->bandWidthMax = 16000;
            _this->sigPath.setDemodulator(SigPath::DEMOD_NFM, _this->bandWidth);
        }
        if (ImGui::RadioButton(CONCAT("WFM##_", _this->name), _this->demod == 1) && _this->demod != 1) {
            _this->demod = 1;
            _this->bandWidth = 200000;
            _this->bandWidthMin = 100000;
            _this->bandWidthMax = 200000;
            _this->sigPath.setDemodulator(SigPath::DEMOD_FM, _this->bandWidth); 
        }
        ImGui::NextColumn();
        if (ImGui::RadioButton(CONCAT("AM##_", _this->name), _this->demod == 2) && _this->demod != 2) {
            _this->demod = 2;
            _this->bandWidth = 12500;
            _this->bandWidthMin = 1500;
            _this->bandWidthMax = 12500;
            _this->sigPath.setDemodulator(SigPath::DEMOD_AM, _this->bandWidth); 
        }
        if (ImGui::RadioButton(CONCAT("DSB##_", _this->name), _this->demod == 3) && _this->demod != 3)  {
            _this->demod = 3;
            _this->bandWidth = 6000;
            _this->bandWidthMin = 3000;
            _this->bandWidthMax = 6000;
            _this->sigPath.setDemodulator(SigPath::DEMOD_DSB, _this->bandWidth); 
        }
        ImGui::NextColumn();
        if (ImGui::RadioButton(CONCAT("USB##_", _this->name), _this->demod == 4) && _this->demod != 4) {
            _this->demod = 4;
            _this->bandWidth = 3000;
            _this->bandWidthMin = 1500;
            _this->bandWidthMax = 3000;
            _this->sigPath.setDemodulator(SigPath::DEMOD_USB, _this->bandWidth); 
        }
        if (ImGui::RadioButton(CONCAT("CW##_", _this->name), _this->demod == 5) && _this->demod != 5) { _this->demod = 5; };
        ImGui::NextColumn();
        if (ImGui::RadioButton(CONCAT("LSB##_", _this->name), _this->demod == 6) && _this->demod != 6) {
            _this->demod = 6;
            _this->bandWidth = 3000;
            _this->bandWidthMin = 1500;
            _this->bandWidthMax = 3000;
            _this->sigPath.setDemodulator(SigPath::DEMOD_LSB, _this->bandWidth);
        }
        if (ImGui::RadioButton(CONCAT("RAW##_", _this->name), _this->demod == 7) && _this->demod != 7) {
            _this->demod = 7;
            _this->bandWidth = 10000;
            _this->bandWidthMin = 3000;
            _this->bandWidthMax = 10000;
            _this->sigPath.setDemodulator(SigPath::DEMOD_RAW, _this->bandWidth);
        };
        ImGui::Columns(1, CONCAT("EndRadioModeColumns##_", _this->name), false);

        ImGui::EndGroup();

        ImGui::Text("WFM Deemphasis");
        ImGui::SameLine();
        ImGui::PushItemWidth(menuColumnWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo(CONCAT("##_deemp_select_", _this->name), &_this->deemp, DEEMP_LIST)) {
            _this->sigPath.setDeemphasis(_this->deemp);
        }
        ImGui::PopItemWidth();

        ImGui::Text("Bandwidth");
        ImGui::SameLine();
        ImGui::PushItemWidth(menuColumnWidth - ImGui::GetCursorPosX());
        if (ImGui::InputInt(CONCAT("##_bw_select_", _this->name), &_this->bandWidth, 100, 1000)) {
            _this->bandWidth = std::clamp<int>(_this->bandWidth, _this->bandWidthMin, _this->bandWidthMax);
            _this->sigPath.setBandwidth(_this->bandWidth);
        }

        ImGui::Text("Squelch");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuColumnWidth - ImGui::GetCursorPosX());
        ImGui::SliderFloat(CONCAT("##_squelch_select_", _this->name), &_this->sigPath.squelch.level, -100, 0);

        ImGui::PopItemWidth();

        ImGui::Text("Snap Interval");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuColumnWidth - ImGui::GetCursorPosX());
        if (ImGui::InputDouble(CONCAT("##_snap_select_", _this->name), &_this->snapInterval)) {
            _this->sigPath.vfo->setSnapInterval(_this->snapInterval);
        }
    }

    static void scriptCreateHandler(void* ctx, duk_context* dukCtx, duk_idx_t objId) {
        duk_push_string(dukCtx, "Hello from modules ;)");
        duk_put_prop_string(dukCtx, objId, "test");

        duk_push_c_function(dukCtx, duk_setDemodulator, 1);
        duk_put_prop_string(dukCtx, objId, "setDemodulator");

        duk_push_c_function(dukCtx, duk_getDemodulator, 0);
        duk_put_prop_string(dukCtx, objId, "getDemodulator");

        duk_push_c_function(dukCtx, duk_setBandwidth, 1);
        duk_put_prop_string(dukCtx, objId, "setBandwidth");

        duk_push_c_function(dukCtx, duk_getBandwidth, 0);
        duk_put_prop_string(dukCtx, objId, "getBandwidth");

        duk_push_c_function(dukCtx, duk_getMaxBandwidth, 0);
        duk_put_prop_string(dukCtx, objId, "getMaxBandwidth");

        duk_push_c_function(dukCtx, duk_getMinBandwidth, 0);
        duk_put_prop_string(dukCtx, objId, "getMinBandwidth");
        
        duk_push_pointer(dukCtx, ctx);
        duk_put_prop_string(dukCtx, objId, DUK_HIDDEN_SYMBOL("radio_ctx"));
    }

    static duk_ret_t duk_setDemodulator(duk_context* dukCtx) {
        const char* str = duk_require_string(dukCtx, -1);
        std::string modName = str;

        duk_push_this(dukCtx);
        if (!duk_get_prop_literal(dukCtx, -1, DUK_HIDDEN_SYMBOL("radio_ctx"))) {
            printf("COULD NOT RETRIEVE POINTER\n");
        }
        
        RadioModule* _this = (RadioModule*)duk_require_pointer(dukCtx, -1);

        duk_pop_n(dukCtx, 3); // Pop demod name, this and context
        
        if (modName == "NFM") { 
            _this->demod = 0;
            _this->bandWidth = 16000;
            _this->bandWidthMin = 8000;
            _this->bandWidthMax = 16000;
            _this->sigPath.setDemodulator(SigPath::DEMOD_NFM, _this->bandWidth);
        }
        else if (modName == "WFM") {
            _this->demod = 1;
            _this->bandWidth = 200000;
            _this->bandWidthMin = 100000;
            _this->bandWidthMax = 200000;
            _this->sigPath.setDemodulator(SigPath::DEMOD_FM, _this->bandWidth); 
        }
        else if (modName == "AM") {
            _this->demod = 2;
            _this->bandWidth = 12500;
            _this->bandWidthMin = 6250;
            _this->bandWidthMax = 12500;
            _this->sigPath.setDemodulator(SigPath::DEMOD_AM, _this->bandWidth); 
        }
        else if (modName == "DSB")  {
            _this->demod = 3;
            _this->bandWidth = 6000;
            _this->bandWidthMin = 3000;
            _this->bandWidthMax = 6000;
            _this->sigPath.setDemodulator(SigPath::DEMOD_DSB, _this->bandWidth); 
        }
        else if (modName == "USB") {
            _this->demod = 4;
            _this->bandWidth = 3000;
            _this->bandWidthMin = 1500;
            _this->bandWidthMax = 3000;
            _this->sigPath.setDemodulator(SigPath::DEMOD_USB, _this->bandWidth); 
        }
        else if (modName == "CW") { _this->demod = 5; }
        else if (modName == "LSB") {
            _this->demod = 6;
            _this->bandWidth = 3000;
            _this->bandWidthMin = 1500;
            _this->bandWidthMax = 3000;
            _this->sigPath.setDemodulator(SigPath::DEMOD_LSB, _this->bandWidth);
        }
        else if (modName == "RAW") {
            _this->demod = 7;
            _this->bandWidth = 10000;
            _this->bandWidthMin = 3000;
            _this->bandWidthMax = 10000;
            _this->sigPath.setDemodulator(SigPath::DEMOD_RAW, _this->bandWidth);
        }
        else {
            duk_push_string(dukCtx, "Invalid demodulator name");
            duk_throw(dukCtx);
        }
        
        return 0;
    }

    static duk_ret_t duk_getDemodulator(duk_context* dukCtx) {
        duk_push_this(dukCtx);
        if (!duk_get_prop_literal(dukCtx, -1, DUK_HIDDEN_SYMBOL("radio_ctx"))) {
            printf("COULD NOT RETRIEVE POINTER\n");
        }
        
        RadioModule* _this = (RadioModule*)duk_require_pointer(dukCtx, -1);

        duk_pop_n(dukCtx, 2); // Pop demod name, this and context

        const char* demodNames[] = {"NFM", "WFM", "AM", "DSB", "USB", "CW", "LSB", "RAW"};

        duk_push_string(dukCtx, demodNames[_this->demod]);

        return 1;
    }

    static duk_ret_t duk_setBandwidth(duk_context* dukCtx) {
        double bandwidth = duk_require_number(dukCtx, -1);

        duk_push_this(dukCtx);
        if (!duk_get_prop_literal(dukCtx, -1, DUK_HIDDEN_SYMBOL("radio_ctx"))) {
            printf("COULD NOT RETRIEVE POINTER\n");
        }
        
        RadioModule* _this = (RadioModule*)duk_require_pointer(dukCtx, -1);

        duk_pop_n(dukCtx, 3); // Pop demod name, this and context

        if (bandwidth > _this->bandWidthMax || bandwidth < _this->bandWidthMin) {
            duk_push_string(dukCtx, "Bandwidth out of range");
            duk_throw(dukCtx);
        }

        _this->bandWidth = bandwidth;
        _this->bandWidth = std::clamp<int>(_this->bandWidth, _this->bandWidthMin, _this->bandWidthMax);
        _this->sigPath.setBandwidth(_this->bandWidth);

        return 0;
    }

    static duk_ret_t duk_getBandwidth(duk_context* dukCtx) {
        duk_push_this(dukCtx);
        if (!duk_get_prop_literal(dukCtx, -1, DUK_HIDDEN_SYMBOL("radio_ctx"))) {
            printf("COULD NOT RETRIEVE POINTER\n");
        }
        
        RadioModule* _this = (RadioModule*)duk_require_pointer(dukCtx, -1);

        duk_pop_n(dukCtx, 2); // Pop demod name, this and context

        duk_push_number(dukCtx, _this->bandWidth);

        return 1;
    }

    static duk_ret_t duk_getMaxBandwidth(duk_context* dukCtx) {
        duk_push_this(dukCtx);
        if (!duk_get_prop_literal(dukCtx, -1, DUK_HIDDEN_SYMBOL("radio_ctx"))) {
            printf("COULD NOT RETRIEVE POINTER\n");
        }
        
        RadioModule* _this = (RadioModule*)duk_require_pointer(dukCtx, -1);

        duk_pop_n(dukCtx, 2); // Pop demod name, this and context

        duk_push_number(dukCtx, _this->bandWidthMax);

        return 1;
    }

    static duk_ret_t duk_getMinBandwidth(duk_context* dukCtx) {
        duk_push_this(dukCtx);
        if (!duk_get_prop_literal(dukCtx, -1, DUK_HIDDEN_SYMBOL("radio_ctx"))) {
            printf("COULD NOT RETRIEVE POINTER\n");
        }
        
        RadioModule* _this = (RadioModule*)duk_require_pointer(dukCtx, -1);

        duk_pop_n(dukCtx, 2); // Pop demod name, this and context

        duk_push_number(dukCtx, _this->bandWidthMin);

        return 1;
    }

    std::string name;
    int demod = 1;
    int deemp = 0;
    int bandWidth;
    int bandWidthMin;
    int bandWidthMax;
    double snapInterval = 100000.0;
    SigPath sigPath;

};

MOD_EXPORT void _INIT_() {
   // Do your one time init here
}

MOD_EXPORT void* _CREATE_INSTANCE_(std::string name) {
    return new RadioModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (RadioModule*)instance;
}

MOD_EXPORT void _STOP_() {
    // Do your one shutdown here
}