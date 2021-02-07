#include <gui/widgets/file_select.h>
#include <regex>
#include <options.h>
#include <filesystem>

FileSelect::FileSelect(std::string defaultPath, char* filter) {
    path = defaultPath;
    strcpy(_filter, filter);
    pathValid = std::filesystem::is_regular_file(path);
    strcpy(strPath, path.c_str());
}

void FileSelect::render(std::string id) {
#ifdef _WIN32
    float menuColumnWidth = ImGui::GetContentRegionAvailWidth();
    float buttonWidth = ImGui::CalcTextSize("...").x + 20.0f;
    bool lastPathValid = pathValid;
    if (!lastPathValid) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
    }
    ImGui::SetNextItemWidth(menuColumnWidth - buttonWidth);
    if (ImGui::InputText(id.c_str(), strPath, 2047)) {
        path = std::string(strPath);
        std::string expandedPath = expandString(strPath);
        if (!std::filesystem::is_regular_file(expandedPath)) {
            pathValid = false;
        }
        else {
            pathValid = true;
            _pathChanged = true;
        }
    }
    if (!lastPathValid) {
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    if (ImGui::Button(("..." + id + "_winselect").c_str(), ImVec2(buttonWidth - 8.0f, 0)) && !dialogOpen) {
        dialogOpen = true;
        if (workerThread.joinable()) { workerThread.join(); }
        workerThread = std::thread(&FileSelect::windowsWorker, this);
    }
#else
    bool lastPathValid = pathValid;
    if (!lastPathValid) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
    }
    if (ImGui::InputText(id.c_str(), strPath, 2047)) {
        path = std::string(strPath);
        std::string expandedPath = expandString(strPath);
        if (!std::filesystem::is_regular_file(expandedPath)) {
            pathValid = false;
        }
        else {
            pathValid = true;
            _pathChanged = true;
        }
    }
    if (!lastPathValid) {
        ImGui::PopStyleColor();
    }
#endif
}

std::string FileSelect::expandString(std::string input) {
    input = std::regex_replace(input, std::regex("%ROOT%"), options::opts.root);
    return std::regex_replace(input, std::regex("//"), "/");
}

bool FileSelect::pathIsValid() {
    return pathValid;
}

bool FileSelect::pathChanged() {
    if (_pathChanged) {
        _pathChanged = false;
        return true;
    }
    return false;
}

#ifdef _WIN32
void FileSelect::windowsWorker() {
        OPENFILENAMEA ofn;       // common dialog box structure
        char szFile[2048];       // buffer for file name
        HWND hwnd;              // owner window
        HANDLE hf;              // file handle

        // Initialize OPENFILENAME
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = GetFocus();
        ofn.lpstrFile = (LPSTR)szFile;
        // Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
        // use the contents of szFile to initialize itself.
        ofn.lpstrFile[0] = '\0';
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = (LPCSTR)_filter;
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = NULL;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = NULL;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_EXTENSIONDIFFERENT;

        // Display the Open dialog box. 

        if (GetOpenFileNameA(&ofn)==TRUE) {
            strcpy(strPath, szFile);
            _pathChanged = true;
        }

        pathValid = std::filesystem::is_regular_file(strPath);
        dialogOpen = false;
}
#endif