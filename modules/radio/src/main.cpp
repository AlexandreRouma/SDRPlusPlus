#include <imgui.h>
#include <module.h>
#include <path.h>
#include <watcher.h>

#define CONCAT(a, b)    ((std::string(a) + b).c_str())
#define DEEMP_LIST      "50µS\00075µS\000none\000"

mod::API_t* API;

struct RadioContext_t {
    std::string name;
    int demod = 1;
    int deemp = 0;
    int bandWidth;
    int bandWidthMin;
    int bandWidthMax;
    SigPath sigPath;
};

MOD_EXPORT void* _INIT_(mod::API_t* _API, ImGuiContext* imctx, std::string _name) {
    API = _API;
    RadioContext_t* ctx = new RadioContext_t;
    ctx->name = _name;
    ctx->bandWidth = 200000;
    ctx->bandWidthMin = 100000;
    ctx->bandWidthMax = 200000;
    ctx->sigPath.init(_name, 200000, 1000, API->registerVFO(_name, mod::API_t::REF_CENTER, 0, 200000, 200000, 1000));
    ctx->sigPath.start();
    ImGui::SetCurrentContext(imctx);
    return ctx;
}

MOD_EXPORT void _NEW_FRAME_(RadioContext_t* ctx) {
    
}

MOD_EXPORT void _DRAW_MENU_(RadioContext_t* ctx) {
    float menuColumnWidth = ImGui::GetContentRegionAvailWidth();

    ImGui::BeginGroup();

    // TODO: Change VFO ref in signal path

    ImGui::Columns(4, CONCAT("RadioModeColumns##_", ctx->name), false);
    if (ImGui::RadioButton(CONCAT("NFM##_", ctx->name), ctx->demod == 0) && ctx->demod != 0) { 
        ctx->demod = 0;
        ctx->bandWidth = 16000;
        ctx->bandWidthMin = 8000;
        ctx->bandWidthMax = 16000;
        ctx->sigPath.setDemodulator(SigPath::DEMOD_NFM, ctx->bandWidth);
        API->setVFOReference(ctx->name, mod::API_t::REF_CENTER);
    }
    if (ImGui::RadioButton(CONCAT("WFM##_", ctx->name), ctx->demod == 1) && ctx->demod != 1) {
        ctx->demod = 1;
        ctx->bandWidth = 200000;
        ctx->bandWidthMin = 100000;
        ctx->bandWidthMax = 200000;
        ctx->sigPath.setDemodulator(SigPath::DEMOD_FM, ctx->bandWidth); 
        API->setVFOReference(ctx->name, mod::API_t::REF_CENTER);
    }
    ImGui::NextColumn();
    if (ImGui::RadioButton(CONCAT("AM##_", ctx->name), ctx->demod == 2) && ctx->demod != 2) {
        ctx->demod = 2;
        ctx->bandWidth = 12500;
        ctx->bandWidthMin = 6250;
        ctx->bandWidthMax = 12500;
        ctx->sigPath.setDemodulator(SigPath::DEMOD_AM, ctx->bandWidth); 
        API->setVFOReference(ctx->name, mod::API_t::REF_CENTER);
    }
    if (ImGui::RadioButton(CONCAT("DSB##_", ctx->name), ctx->demod == 3) && ctx->demod != 3)  {
        ctx->demod = 3;
        ctx->bandWidth = 6000;
        ctx->bandWidthMin = 3000;
        ctx->bandWidthMax = 6000;
        ctx->sigPath.setDemodulator(SigPath::DEMOD_DSB, ctx->bandWidth); 
        API->setVFOReference(ctx->name, mod::API_t::REF_CENTER);
    }
    ImGui::NextColumn();
    if (ImGui::RadioButton(CONCAT("USB##_", ctx->name), ctx->demod == 4) && ctx->demod != 4) {
        ctx->demod = 4;
        ctx->bandWidth = 3000;
        ctx->bandWidthMin = 1500;
        ctx->bandWidthMax = 3000;
        ctx->sigPath.setDemodulator(SigPath::DEMOD_USB, ctx->bandWidth); 
        API->setVFOReference(ctx->name, mod::API_t::REF_LOWER);
    }
    if (ImGui::RadioButton(CONCAT("CW##_", ctx->name), ctx->demod == 5) && ctx->demod != 5) { ctx->demod = 5; };
    ImGui::NextColumn();
    if (ImGui::RadioButton(CONCAT("LSB##_", ctx->name), ctx->demod == 6) && ctx->demod != 6) {
        ctx->demod = 6;
        ctx->bandWidth = 3000;
        ctx->bandWidthMin = 1500;
        ctx->bandWidthMax = 3000;
        ctx->sigPath.setDemodulator(SigPath::DEMOD_LSB, ctx->bandWidth);
        API->setVFOReference(ctx->name, mod::API_t::REF_UPPER);
    }
    if (ImGui::RadioButton(CONCAT("RAW##_", ctx->name), ctx->demod == 7) && ctx->demod != 7) { ctx->demod = 7; };
    ImGui::Columns(1, CONCAT("EndRadioModeColumns##_", ctx->name), false);

    ImGui::EndGroup();

    ImGui::Text("WFM Deemphasis");
    ImGui::SameLine();
    ImGui::PushItemWidth(menuColumnWidth - ImGui::GetCursorPosX());
    if (ImGui::Combo(CONCAT("##_deemp_select_", ctx->name), &ctx->deemp, DEEMP_LIST)) {
        ctx->sigPath.setDeemphasis(ctx->deemp);
    }
    ImGui::PopItemWidth();

    ImGui::Text("Bandwidth");
    ImGui::SameLine();
    ImGui::PushItemWidth(menuColumnWidth - ImGui::GetCursorPosX());
    if (ImGui::InputInt(CONCAT("##_bw_select_", ctx->name), &ctx->bandWidth, 100, 1000)) {
        ctx->bandWidth = std::clamp<int>(ctx->bandWidth, ctx->bandWidthMin, ctx->bandWidthMax);
        ctx->sigPath.setBandwidth(ctx->bandWidth);
    }
    ImGui::PopItemWidth();
}

MOD_EXPORT void _HANDLE_EVENT_(RadioContext_t* ctx, int eventId) {
    
}

MOD_EXPORT void _STOP_(RadioContext_t* ctx) {
    API->removeVFO(ctx->name);
    delete ctx;
}