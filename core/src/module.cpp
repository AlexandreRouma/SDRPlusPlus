#include <module.h>
#include <filesystem>
#include <spdlog/spdlog.h>

ModuleManager::Module_t ModuleManager::loadModule(std::string path) {
    Module_t mod;
    if (!std::filesystem::exists(path)) {
        spdlog::error("{0} does not exist", path);
        mod.handle = NULL;
        return mod;
    }
    if (!std::filesystem::is_regular_file(path)) {
        spdlog::error("{0} isn't a loadable module", path);
        mod.handle = NULL;
        return mod;
    }
#ifdef _WIN32
    mod.handle = LoadLibraryA(path.c_str());
    if (mod.handle == NULL) {
        spdlog::error("Couldn't load {0}. Error code: {1}", path, GetLastError());
        mod.handle = NULL;
        return mod;
    }
    mod.info = (ModuleInfo_t*)GetProcAddress(mod.handle, "_INFO_");
    mod.init = (void(*)())GetProcAddress(mod.handle, "_INIT_");
    mod.createInstance = (Instance*(*)(std::string))GetProcAddress(mod.handle, "_CREATE_INSTANCE_");
    mod.deleteInstance = (void(*)(Instance*))GetProcAddress(mod.handle, "_DELETE_INSTANCE_");
    mod.end = (void(*)())GetProcAddress(mod.handle, "_END_");
#else
    mod.handle = dlopen(path.c_str(), RTLD_LAZY);
    if (mod.handle == NULL) {
        spdlog::error("Couldn't load {0}.", path);
        mod.handle = NULL;
        return mod;
    }
    mod.info = (ModuleInfo_t*)dlsym(mod.handle, "_INFO_");
    mod.init = (void(*)())dlsym(mod.handle, "_INIT_");
    mod.createInstance = (Instance*(*)(std::string))dlsym(mod.handle, "_CREATE_INSTANCE_");
    mod.deleteInstance = (void(*)(Instance*))dlsym(mod.handle, "_DELETE_INSTANCE_");
    mod.end = (void(*)())dlsym(mod.handle, "_END_");
#endif
    if (mod.info == NULL) {
        spdlog::error("{0} is missing _INFO_ symbol", path);
        mod.handle = NULL;
        return mod;
    }
    if (mod.init == NULL) {
        spdlog::error("{0} is missing _INIT_ symbol", path);
        mod.handle = NULL;
        return mod;
    }
    if (mod.createInstance == NULL) {
        spdlog::error("{0} is missing _CREATE_INSTANCE_ symbol", path);
        mod.handle = NULL;
        return mod;
    }
    if (mod.deleteInstance == NULL) {
        spdlog::error("{0} is missing _DELETE_INSTANCE_ symbol", path);
        mod.handle = NULL;
        return mod;
    }
    if (mod.end == NULL) {
        spdlog::error("{0} is missing _END_ symbol", path);
        mod.handle = NULL;
        return mod;
    }
    if (modules.find(mod.info->name) != modules.end()) {
        spdlog::error("{0} has the same name as an already loaded module", path);
        mod.handle = NULL;
        return mod;
    }
    for (auto const& [name, _mod] : modules) {
        if (mod.handle == _mod.handle) {
            return _mod;
        }
    }
    mod.init();
    modules[mod.info->name] = mod;
    return mod;
}

void ModuleManager::createInstance(std::string name, std::string module) {
    if (modules.find(module) == modules.end()) {
        spdlog::error("Module '{0}' doesn't exist", module);
        return;
    }
    if (instances.find(name) != instances.end()) {
        spdlog::error("A module instance with the name '{0}' already exists", name);
        return;
    }
    int maxCount = modules[module].info->maxInstances;
    if (countModuleInstances(module) >= maxCount && maxCount > 0) {
        spdlog::error("Maximum number of instances reached for '{0}'", module);
        return;
    }
    Instance_t inst;
    inst.module = modules[module];
    inst.instance = inst.module.createInstance(name);
    instances[name] = inst;
}

void ModuleManager::deleteInstance(std::string name) {
    if (instances.find(name) == instances.end()) {
        spdlog::error("Tried to remove non-existant instance '{0}'", name);
        return;
    }
    Instance_t inst  = instances[name];
    inst.module.deleteInstance(inst.instance);
    instances.erase(name);
}

void ModuleManager::deleteInstance(ModuleManager::Instance* instance) {
    spdlog::error("Delete instance not implemented");
}

void ModuleManager::enableInstance(std::string name) {
    if (instances.find(name) == instances.end()) {
        spdlog::error("Cannot enable '{0}', instance doesn't exist", name);
        return;
    }
    instances[name].instance->enable();
}

void ModuleManager::disableInstance(std::string name) {
    if (instances.find(name) == instances.end()) {
        spdlog::error("Cannot disable '{0}', instance doesn't exist", name);
        return;
    }
    instances[name].instance->disable();
}

bool ModuleManager::instanceEnabled(std::string name) {
    if (instances.find(name) == instances.end()) {
        spdlog::error("Cannot check if '{0}' is enabled, instance doesn't exist", name);
        return false;
    }
    return instances[name].instance->isEnabled();
}

int ModuleManager::countModuleInstances(std::string module) {
    if (modules.find(module) == modules.end()) {
        spdlog::error("Cannot count instances of '{0}', Module doesn't exist", module);
        return -1;
    }
    ModuleManager::Module_t mod = modules[module];
    int count = 0;
    for (auto const& [name, instance] : instances) {
        if (instance.module == mod) { count++; }
    }
    return count;
}