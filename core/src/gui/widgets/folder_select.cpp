#include <gui/widgets/folder_select.h>
#include <regex>
#include <options.h>
#include <filesystem>

FolderSelect::FolderSelect(std::string defaultPath) {
    setPath(defaultPath);
}

bool FolderSelect::render(std::string id) {
    bool _pathChanged = false;
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
        if (!std::filesystem::is_directory(expandedPath)) {
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
        workerThread = std::thread(&FolderSelect::windowsWorker, this);
    }
#else
    bool lastPathValid = pathValid;
    if (!lastPathValid) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
    }
    if (ImGui::InputText(id.c_str(), strPath, 2047)) {
        path = std::string(strPath);
        std::string expandedPath = expandString(strPath);
        if (!std::filesystem::is_directory(expandedPath)) {
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

void FolderSelect::setPath(std::string path) {
    this->path = path;
    std::string expandedPath = expandString(path);
    pathValid = std::filesystem::is_directory(expandedPath);
    strcpy(strPath, path.c_str());
}

std::string FolderSelect::expandString(std::string input) {
    input = std::regex_replace(input, std::regex("%ROOT%"), options::opts.root);
    return std::regex_replace(input, std::regex("//"), "/");
}

bool FolderSelect::pathIsValid() {
    return pathValid;
}

#ifdef _WIN32
void FolderSelect::windowsWorker() {
        IFileOpenDialog *pFileOpen;
        HRESULT hr;

        // Create the FileOpenDialog object.
        hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pFileOpen));
        
        DWORD options;
        pFileOpen->GetOptions(&options);
        pFileOpen->SetOptions(options | FOS_PICKFOLDERS);

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

        pathValid = std::filesystem::is_directory(path);
        dialogOpen = false;
}
#endif