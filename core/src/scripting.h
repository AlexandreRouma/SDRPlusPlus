#pragma once
#include <string>
#include <thread>
#include <vector>
#include <map>
#include <duktape/duktape.h>

class ScriptManager {
public:
    ScriptManager();

    friend class Script;

    class Script {
    public:
        Script(ScriptManager* man, std::string name, std::string path);
        void run();
        bool running = false;

    private:
        static void scriptWorker(Script* _this);

        duk_context* ctx;
        std::thread worker;
        std::string code;
        std::string name;
        

        ScriptManager* manager;

    };

    struct ScriptRunHandler_t {
        void (*handler)(void* ctx, duk_context* dukCtx, duk_idx_t objId);
        void* ctx;
    };

    void bindScriptRunHandler(std::string name, ScriptManager::ScriptRunHandler_t handler);
    ScriptManager::Script* createScript(std::string name, std::string path);

    std::map<std::string, ScriptManager::Script*> scripts;

private:
    static duk_context* createContext(ScriptManager* _this, std::string name);

    // API
    static duk_ret_t duk_setSource(duk_context* ctx);

    std::map<std::string, ScriptManager::ScriptRunHandler_t> handlers;

};