#include "radio_module.h"
#include <options.h>

SDRPP_MOD_INFO {
    /* Name:            */ "radio",
    /* Description:     */ "Analog radio decoder",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 2, 0, 0,
    /* Max instances    */ -1
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    config.setPath(options::opts.root + "/radio_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new RadioModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (RadioModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}