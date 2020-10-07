#include <scripting.h>
#include <duktape/duk_console.h>
#include <version.h>
#include <fstream>
#include <sstream>

ScriptManager::ScriptManager() {

}

ScriptManager::Script* ScriptManager::createScript(std::string name, std::string path) {
    ScriptManager::Script* script =  new ScriptManager::Script(this, name, path);
    scripts[name] = script;
    return script;
}

ScriptManager::Script::Script(ScriptManager* man, std::string name, std::string path) {
    this->name = name;
    manager = man;
    std::ifstream file(path, std::ios::in);
    std::stringstream ss;
    ss << file.rdbuf();
    code = ss.str(); 
}

void ScriptManager::Script::run() {
    ctx = ScriptManager::createContext(manager, name);
    running = true;
    if (worker.joinable()) {
        worker.join();
    }
    worker = std::thread(scriptWorker, this);
}

duk_context* ScriptManager::createContext(ScriptManager* _this, std::string name) {
    duk_context* ctx = duk_create_heap_default();

    duk_console_init(ctx, DUK_CONSOLE_PROXY_WRAPPER);

    duk_push_string(ctx, name.c_str());
    duk_put_global_string(ctx, "SCRIPT_NAME");

    duk_idx_t sdrppBase = duk_push_object(ctx);

    duk_push_string(ctx, VERSION_STR);
    duk_put_prop_string(ctx, sdrppBase, "version");

    duk_idx_t modObjId = duk_push_object(ctx);
    for (const auto& [name, handler] : _this->handlers) {
        duk_idx_t objId = duk_push_object(ctx);
        handler.handler(handler.ctx, ctx, objId);
        duk_put_prop_string(ctx, modObjId, name.c_str());
    }
    duk_put_prop_string(ctx, sdrppBase, "modules");

    duk_put_global_string(ctx, "sdrpp");

    return ctx;
}

// TODO: Switch to spdlog

void ScriptManager::Script::scriptWorker(Script* script) {
    if (duk_peval_string(script->ctx, script->code.c_str()) != 0) {
        printf("Error: %s\n", duk_safe_to_string(script->ctx, -1));
        //return;
    }
    duk_destroy_heap(script->ctx);
    script->running = false;
}

void ScriptManager::bindScriptRunHandler(std::string name, ScriptManager::ScriptRunHandler_t handler) {
    // TODO: check if it exists and add a "unbind" function
    handlers[name] = handler;
}