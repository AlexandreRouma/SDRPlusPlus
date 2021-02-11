#include <gui/widgets/file_select.h>
#include <regex>
#include <options.h>
#include <filesystem>

FileSelect::FileSelect(std::string defaultPath) {
    path = defaultPath;
    pathValid = std::filesystem::is_regular_file(path);
    strcpy(strPath, path.c_str());
}

bool FileSelect::render(std::string id) {
    bool _pathChanged = false;
    float menuColumnWidth = ImGui::GetContentRegionAvailWidth();
#ifdef _WIN32
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
    ImGui::SetNextItemWidth(menuColumnWidth);
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
    _pathChanged |= pathChanged;
    pathChanged = false;
    return _pathChanged;
}

void FileSelect::setPath(std::string path) {
    this->path = path;
    pathValid = std::filesystem::is_regular_file(path);
    strcpy(strPath, path.c_str());
}

std::string FileSelect::expandString(std::string input) {
    input = std::regex_replace(input, std::regex("%ROOT%"), options::opts.root);
    return std::regex_replace(input, std::regex("//"), "/");
}

bool FileSelect::pathIsValid() {
    return pathValid;
}

#ifdef _WIN32

void FileSelect::setWindowsFilter(COMDLG_FILTERSPEC* filt, int n) {
    filter = filt;
    filterCount = n;
}

void FileSelect::windowsWorker() {
        IFileOpenDialog *pFileOpen;
        HRESULT hr;

        // Create the FileOpenDialog object.
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
        
        if (filter != NULL) { pFileOpen->SetFileTypes(filterCount, filter); }

        hr = pFileOpen->Show(NULL);
        if (SUCCEEDED(hr)) {
            PWSTR pszFilePath;
            IShellItem *pItem;
            hr = pFileOpen->GetResult(&pItem);
            hr = pItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);
            wcstombs(strPath, pszFilePath, 2048);
            CoTaskMemFree(pszFilePath);
            path = std::string(strPath);
            pathChanged = true;
        }

        pathValid = std::filesystem::is_regular_file(path);
        dialogOpen = false;
}
#endif