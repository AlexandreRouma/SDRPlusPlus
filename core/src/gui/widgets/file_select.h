#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <stdint.h>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#include <thread>
#endif

class FileSelect {
public:
    FileSelect(std::string defaultPath, char* filter = "All\0*.*");
    void render(std::string id);
    bool pathIsValid();
    bool pathChanged();

    std::string expandString(std::string input);

    std::string path = "";

private:
#ifdef _WIN32
    void windowsWorker();
    std::thread workerThread;
#endif

    char _filter[2048];
    bool pathValid = false;
    bool dialogOpen = false;
    char strPath[2048];
    bool _pathChanged = false;
};