#include <gui/widgets/folder_select.h>
#include <regex>
#include <filesystem>
#include <gui/file_dialogs.h>
#include <core.h>

FolderSelect::FolderSelect(std::string defaultPath) {
    root = core::args["root"];
    setPath(defaultPath);
}

bool FolderSelect::render(std::string id) {
    bool _pathChanged = false;
    float menuColumnWidth = ImGui::GetContentRegionAvail().x;

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
        workerThread = std::thread(&FolderSelect::worker, this);
    }

    _pathChanged |= pathChanged;
    pathChanged = false;
    return _pathChanged;
}

void FolderSelect::setPath(std::string path, bool markChanged) {
    this->path = path;
    std::string expandedPath = expandString(path);
    pathValid = std::filesystem::is_directory(expandedPath);
    if (markChanged) { pathChanged = true; }
    strcpy(strPath, path.c_str());
}

std::string FolderSelect::expandString(std::string input) {
    input = std::regex_replace(input, std::regex("%ROOT%"), root);
    return std::regex_replace(input, std::regex("//"), "/");
}

bool FolderSelect::pathIsValid() {
    return pathValid;
}

void FolderSelect::worker() {
    auto fold = pfd::select_folder("Select Folder", pathValid ? std::filesystem::path(expandString(path)).parent_path().string() : "");
    std::string res = fold.result();

    if (res != "") {
        path = res;
        strcpy(strPath, path.c_str());
        pathChanged = true;
    }

    pathValid = std::filesystem::is_directory(expandString(path));
    dialogOpen = false;
}