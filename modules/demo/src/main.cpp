#include <imgui.h>
#include <module.h>
#include <watcher.h>
#include <wav.h>
#include <dsp/types.h>
#include <dsp/stream.h>
#include <thread>
#include <ctime>

mod::API_t* API;

struct ExampleContext_t {
    std::string name;
};

MOD_EXPORT void* _INIT_(mod::API_t* _API, ImGuiContext* imctx, std::string _name) {
    API = _API;
    ExampleContext_t* ctx = new ExampleContext_t;
    ctx->name = _name;
    ImGui::SetCurrentContext(imctx);
    return ctx;
}

MOD_EXPORT void _NEW_FRAME_(ExampleContext_t* ctx) {
    
}

MOD_EXPORT void _DRAW_MENU_(ExampleContext_t* ctx) {
    ImGui::Text("Demo!");
}

MOD_EXPORT void _HANDLE_EVENT_(ExampleContext_t* ctx, int eventId) {
    
}

MOD_EXPORT void _STOP_(ExampleContext_t* ctx) {
    
}