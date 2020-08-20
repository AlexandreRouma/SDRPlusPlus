#include <imgui.h>
#include <module.h>
#include <path.h>
#include <watcher.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

mod::API_t* API;

struct RadioContext_t {
    std::string name;
    int demod = 1;
    SigPath sigPath;
};

MOD_EXPORT void* _INIT_(mod::API_t* _API, ImGuiContext* imctx, std::string _name) {
    API = _API;
    RadioContext_t* ctx = new RadioContext_t;
    ctx->name = _name;
    ctx->sigPath.init(_name, 200000, 1000, API->registerVFO(_name, mod::API_t::REF_CENTER, 0, 200000, 200000, 1000));
    ctx->sigPath.start();
    ImGui::SetCurrentContext(imctx);
    return ctx;
}

MOD_EXPORT void _NEW_FRAME_(RadioContext_t* ctx) {
    
}

MOD_EXPORT void _DRAW_MENU_(RadioContext_t* ctx) {
    ImGui::BeginGroup();

    ImGui::Columns(4, CONCAT("RadioModeColumns##_", ctx->name), false);
    if (ImGui::RadioButton(CONCAT("NFM##_", ctx->name), ctx->demod == 0) && ctx->demod != 0) { 
        ctx->sigPath.setDemodulator(SigPath::DEMOD_NFM);
        ctx->demod = 0; 
        API->setVFOReference(ctx->name, mod::API_t::REF_CENTER);
    }
    if (ImGui::RadioButton(CONCAT("WFM##_", ctx->name), ctx->demod == 1) && ctx->demod != 1) { 
        ctx->sigPath.setDemodulator(SigPath::DEMOD_FM); 
        ctx->demod = 1; 
        API->setVFOReference(ctx->name, mod::API_t::REF_CENTER);
    }
    ImGui::NextColumn();
    if (ImGui::RadioButton(CONCAT("AM##_", ctx->name), ctx->demod == 2) && ctx->demod != 2) { 
        ctx->sigPath.setDemodulator(SigPath::DEMOD_AM); 
        ctx->demod = 2; 
        API->setVFOReference(ctx->name, mod::API_t::REF_CENTER);
    }
    if (ImGui::RadioButton(CONCAT("DSB##_", ctx->name), ctx->demod == 3) && ctx->demod != 3)  { 
        ctx->sigPath.setDemodulator(SigPath::DEMOD_DSB); 
        ctx->demod = 3; 
        API->setVFOReference(ctx->name, mod::API_t::REF_CENTER);
    }
    ImGui::NextColumn();
    if (ImGui::RadioButton(CONCAT("USB##_", ctx->name), ctx->demod == 4) && ctx->demod != 4) { 
        ctx->sigPath.setDemodulator(SigPath::DEMOD_USB); 
        ctx->demod = 4; 
        API->setVFOReference(ctx->name, mod::API_t::REF_LOWER);
    }
    if (ImGui::RadioButton(CONCAT("CW##_", ctx->name), ctx->demod == 5) && ctx->demod != 5) { ctx->demod = 5; };
    ImGui::NextColumn();
    if (ImGui::RadioButton(CONCAT("LSB##_", ctx->name), ctx->demod == 6) && ctx->demod != 6) {
        ctx->sigPath.setDemodulator(SigPath::DEMOD_LSB);
        ctx->demod = 6;
        API->setVFOReference(ctx->name, mod::API_t::REF_UPPER);
    }
    if (ImGui::RadioButton(CONCAT("RAW##_", ctx->name), ctx->demod == 7) && ctx->demod != 7) { ctx->demod = 7; };
    ImGui::Columns(1, CONCAT("EndRadioModeColumns##_", ctx->name), false);

    ImGui::EndGroup();

    ImGui::Checkbox(CONCAT("Deemphasis##_", ctx->name), &ctx->sigPath.deemp.bypass);
}

MOD_EXPORT void _HANDLE_EVENT_(RadioContext_t* ctx, int eventId) {
    
}

MOD_EXPORT void _STOP_(RadioContext_t* ctx) {
    API->removeVFO(ctx->name);
    delete ctx;
}