#include <imgui.h>
#include <module.h>
#include <watcher.h>
#include <dsp/types.h>
#include <dsp/stream.h>
#include <thread>
#include <ctime>
#include <stdio.h>
#include <style.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())


struct DemoContext_t {
    std::string name;
};

MOD_EXPORT void* _INIT_(mod::API_t* _API, ImGuiContext* imctx, std::string _name) {
    DemoContext_t* ctx = new DemoContext_t;
    ctx->name = _name;
    return ctx;
}

MOD_EXPORT void _NEW_FRAME_(DemoContext_t* ctx) {
    
}

MOD_EXPORT void _DRAW_MENU_(DemoContext_t* ctx) {
    ImGui::Text(ctx->name.c_str());
}

MOD_EXPORT void _HANDLE_EVENT_(DemoContext_t* ctx, int eventId) {
    
}

MOD_EXPORT void _STOP_(DemoContext_t* ctx) {
    
}