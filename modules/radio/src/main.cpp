#include <imgui.h>
#include <module.h>
#include <path.h>

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

MOD_EXPORT void _DRAW_MENU_(RadioContext_t* ctx) {
    ImGui::BeginGroup();

    ImGui::Columns(4, CONCAT("RadioModeColumns##_", ctx->name), false);
    if (ImGui::RadioButton(CONCAT("NFM##_", ctx->name), ctx->demod == 0) && ctx->demod != 0) { 
        ctx->sigPath.setDemodulator(SigPath::DEMOD_NFM);
        ctx->demod = 0; 
        API->setVFOBandwidth(ctx->name, 12500); 
        // vfo->setReference(ImGui::WaterFall::REF_CENTER);
    }
    if (ImGui::RadioButton(CONCAT("WFM##_", ctx->name), ctx->demod == 1) && ctx->demod != 1) { 
        ctx->sigPath.setDemodulator(SigPath::DEMOD_FM); 
        ctx->demod = 1; 
        API->setVFOBandwidth(ctx->name, 200000);
        // vfo->setReference(ImGui::WaterFall::REF_CENTER);
    }
    ImGui::NextColumn();
    if (ImGui::RadioButton(CONCAT("AM##_", ctx->name), ctx->demod == 2) && ctx->demod != 2) { 
        ctx->sigPath.setDemodulator(SigPath::DEMOD_AM); 
        ctx->demod = 2; 
        API->setVFOBandwidth(ctx->name, 12500);
        // vfo->setReference(ImGui::WaterFall::REF_CENTER);
    }
    if (ImGui::RadioButton(CONCAT("DSB##_", ctx->name), ctx->demod == 3) && ctx->demod != 3) { ctx->demod = 3; };
    ImGui::NextColumn();
    if (ImGui::RadioButton(CONCAT("USB##_", ctx->name), ctx->demod == 4) && ctx->demod != 4) { 
        ctx->sigPath.setDemodulator(SigPath::DEMOD_USB); 
        ctx->demod = 4; 
        API->setVFOBandwidth(ctx->name, 3000);
        // vfo->setReference(ImGui::WaterFall::REF_LOWER);
    }
    if (ImGui::RadioButton(CONCAT("CW##_", ctx->name), ctx->demod == 5) && ctx->demod != 5) { ctx->demod = 5; };
    ImGui::NextColumn();
    if (ImGui::RadioButton(CONCAT("LSB##_", ctx->name), ctx->demod == 6) && ctx->demod != 6) {
        ctx->sigPath.setDemodulator(SigPath::DEMOD_LSB);
        ctx->demod = 6;
        API->setVFOBandwidth(ctx->name, 3000);
        // vfo->setReference(ImGui::WaterFall::REF_UPPER);
    }
    if (ImGui::RadioButton(CONCAT("RAW##_", ctx->name), ctx->demod == 7) && ctx->demod != 7) { ctx->demod = 7; };
    ImGui::Columns(1, CONCAT("EndRadioModeColumns##_", ctx->name), false);

    ImGui::EndGroup();
}

MOD_EXPORT void _HANDLE_EVENT_(RadioContext_t* ctx, int eventId) {
    if (eventId == mod::EVENT_STREAM_PARAM_CHANGED) {
       ctx->sigPath.updateBlockSize();
    }
}

MOD_EXPORT void _STOP_(RadioContext_t* ctx) {
    API->removeVFO(ctx->name);
    delete ctx;
}