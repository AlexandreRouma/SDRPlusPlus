#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <stdint.h>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#include <ShlObj.h>
#include <thread>
#endif

class FolderSelect {
public:
    FolderSelect(std::string defaultPath);
    bool render(std::string id);
    void setPath(std::string path);
    bool pathIsValid();

    std::string expandString(std::string input);

    std::string path = "";
    

private:
#ifdef _WIN32
    void windowsWorker();
    std::thread workerThread;
#endif

    bool pathValid = false;
    bool dialogOpen = false;
    char strPath[2048];
    bool pathChanged = false;
};