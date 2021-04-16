#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <stdint.h>
#include <string>

#ifdef _WIN32
#include <Windows.h>
#include <thread>
#include <ShlObj.h>
#endif

class FileSelect {
public:
    FileSelect(std::string defaultPath);
    bool render(std::string id);
    void setPath(std::string path);
    bool pathIsValid();

    std::string expandString(std::string input);

    std::string path = "";

#ifdef _WIN32
    void setWindowsFilter(COMDLG_FILTERSPEC* filt, int n);
#endif

private:
#ifdef _WIN32
    void windowsWorker();
    std::thread workerThread;
    COMDLG_FILTERSPEC* filter = NULL;
    int filterCount = 0;
#endif

    char _filter[2048];
    bool pathValid = false;
    bool dialogOpen = false;
    char strPath[2048];
    bool pathChanged = false;
};