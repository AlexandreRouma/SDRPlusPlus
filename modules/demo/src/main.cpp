#include <imgui.h>
#include <module.h>

mod::API_t* API;

struct DemoContext_t {
    std::string name;
};

MOD_EXPORT void* _INIT_(mod::API_t* _API, ImGuiContext* imctx, std::string _name) {
    API = _API;
    DemoContext_t* ctx = new DemoContext_t;
    ctx->name = _name;
    ImGui::SetCurrentContext(imctx);
    return ctx;
}

MOD_EXPORT void _DRAW_MENU_(DemoContext_t* ctx) {
    char buf[100];
    sprintf(buf, "I'm %s", ctx->name.c_str());
    ImGui::Button(buf);
}

MOD_EXPORT void _STOP_(DemoContext_t* ctx) {
    delete ctx;
}