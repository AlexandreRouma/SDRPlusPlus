#include <gui/widgets/file_select.h>
#include <regex>
#include <options.h>
#include <filesystem>
#include <gui/file_dialogs.h>

FileSelect::FileSelect(std::string defaultPath, std::vector<std::string> filter) {
    _filter = filter;
    setPath(defaultPath);
}

bool FileSelect::render(std::string id) {
    bool _pathChanged = false;
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
        workerThread = std::thread(&FileSelect::worker, this);
    }
    
    _pathChanged |= pathChanged;
    pathChanged = false;
    return _pathChanged;
}

void FileSelect::setPath(std::string path) {
    this->path = path;
    std::string expandedPath = expandString(path);
    pathValid = std::filesystem::is_regular_file(expandedPath);
    strcpy(strPath, path.c_str());
}

std::string FileSelect::expandString(std::string input) {
    input = std::regex_replace(input, std::regex("%ROOT%"), options::opts.root);
    return std::regex_replace(input, std::regex("//"), "/");
}

bool FileSelect::pathIsValid() {
    return pathValid;
}

void FileSelect::worker() {
        auto file = pfd::open_file("Open File", "", _filter);
        std::vector<std::string> res = file.result();

        if (res.size() > 0) {
            path = res[0];
            strcpy(strPath, path.c_str());
            pathChanged = true;
        }

        pathValid = std::filesystem::is_regular_file(expandString(path));
        dialogOpen = false;
}