#include <gui/menus/source.h>
#include <imgui.h>
#include <gui/gui.h>
#include <core.h>
#include <gui/main_window.h>
#include <gui/style.h>
#include <signal_path/signal_path.h>
#include <utils/optionlist.h>
#include <gui/dialogs/dialog_box.h>

namespace sourcemenu {
    int offsetMode = 0;
    int sourceId = 0;
    double customOffset = 0.0;
    double effectiveOffset = 0.0;
    int decimId = 0;
    bool iqCorrection = false;
    bool invertIQ = false;

    EventHandler<std::string> sourcesChangedHandler;
    EventHandler<std::string> sourceUnregisterHandler;

    OptionList<std::string, std::string> sources;
    std::string selectedSource;
    OptionList<std::string, int> offsets;
    std::vector<double> customOffsets;
    OptionList<int, int> decimations;

    bool showAddOffsetDialog = false;
    char newOffsetName[1024];
    double newOffset = 0.0;

    bool showDelOffsetDialog = false;
    std::string delOffsetName = "";

    enum {
        OFFSET_MODE_NONE,
        OFFSET_MODE_MANUAL,
        OFFSET_MODE_CUSTOM_BASE
    };

    void updateOffset() {
        // Compute the effective offset
        switch (offsetMode) {
        case OFFSET_MODE_NONE:
            effectiveOffset = 0;
            break;
        case OFFSET_MODE_MANUAL:
            effectiveOffset = customOffset;
            break;
        default:
            effectiveOffset = customOffsets[offsetMode - OFFSET_MODE_CUSTOM_BASE];
            break;
        }

        // Apply it
        sigpath::sourceManager.setTuningOffset(effectiveOffset);
    }

    void refreshSources() {
        // Get sources
        auto sourceNames = sigpath::sourceManager.getSourceNames();

        // Define source options
        sources.clear();
        for (auto name : sourceNames) {
            sources.define(name, name, name);
        }
    }

    void selectSource(std::string name) {
        // If there is no source, give up
        if (sources.empty()) {
            sourceId = 0;
            selectedSource.clear();
            return;
        }

        // If a source with the given name doesn't exist, select the first source instead
        if (!sources.valueExists(name)) {
            selectSource(sources.value(0));
            return;
        }

        // Update the GUI variables
        sourceId = sources.valueExists(name);
        selectedSource = name;

        // Select the source module
        sigpath::sourceManager.selectSource(name);
    }

    void onSourcesChanged(std::string name, void* ctx) {
        // Update the source list
        refreshSources();

        // Reselect the current source
        selectSource(selectedSource);
    }

    void onSourceUnregister(std::string name, void* ctx) {
        if (name != selectedSource) { return; }

        // TODO: Stop everything
    }

    void loadOffsets() {
        // Clear list
        offsets.clear();
        customOffsets.clear();

        // Define special offset modes
        offsets.define("None", 0);
        offsets.define("Manual", 1);

        // Acquire the config file
        core::configManager.acquire();

        // Load custom offsets
        std::vector<json> offsetList = core::configManager.conf["offsets"];
        for (auto& o : offsetList) {
            customOffsets.push_back(o["offset"]);
            offsets.define(o["name"], offsets.size());
        }

        // Release the config file
        core::configManager.release();
    }

    void init() {
        // Load offset modes
        loadOffsets();

        // Define decimation values
        decimations.define(1, "None", 1);
        decimations.define(2, "2x", 2);
        decimations.define(4, "4x", 4);
        decimations.define(8, "8x", 8);
        decimations.define(16, "16x", 16);
        decimations.define(32, "32x", 32);
        decimations.define(64, "64x", 64);

        // Acquire the config file
        core::configManager.acquire();

        // Load other settings
        std::string selected = core::configManager.conf["source"];
        customOffset = core::configManager.conf["offset"];
        offsetMode = core::configManager.conf["offsetMode"];
        iqCorrection = core::configManager.conf["iqCorrection"];
        invertIQ = core::configManager.conf["invertIQ"];
        int decimation = core::configManager.conf["decimation"];
        if (decimations.keyExists(decimation)) {
            decimId = decimations.keyId(decimation);
        }

        // Release the config file
        core::configManager.release();

        // Select the source module
        refreshSources();
        selectSource(selected);

        // Update frontend settings
        sigpath::iqFrontEnd.setDCBlocking(iqCorrection);
        sigpath::iqFrontEnd.setInvertIQ(invertIQ);
        updateOffset();
        sigpath::iqFrontEnd.setDecimation(decimations.value(decimId));

        // Register handlers
        sourcesChangedHandler.handler = onSourcesChanged;
        sourceUnregisterHandler.handler = onSourceUnregister;
        sigpath::sourceManager.onSourceRegistered.bindHandler(&sourcesChangedHandler);
        sigpath::sourceManager.onSourceUnregister.bindHandler(&sourceUnregisterHandler);
        sigpath::sourceManager.onSourceUnregistered.bindHandler(&sourcesChangedHandler);
    }

    void addOffset(const std::string& name, double offset) {
        // Acquire the config file
        core::configManager.acquire();

        // Define a new offset
        auto newOffsetObj = json::object();
        newOffsetObj["name"] = newOffsetName;
        newOffsetObj["offset"] = newOffset;
        core::configManager.conf["offsets"].push_back(newOffsetObj);

        // Acquire the config file
        core::configManager.release(true);

        // Reload the offsets
        loadOffsets();

        // Re-select the same one
        // TODO: Switch from an array to a map, because two can't have the same name anyway and it'll just make things easier...+
    }

    void delOffset(const std::string& name) {
        
    }

    bool addOffsetDialog() {
        bool open = true;
        gui::mainWindow.lockWaterfallControls = true;

        float menuWidth = ImGui::GetContentRegionAvail().x;

        const char* id = "Add offset##sdrpp_add_offset_dialog_";
        ImGui::OpenPopup(id);

        if (ImGui::BeginPopup(id, ImGuiWindowFlags_NoResize)) {
            ImGui::LeftLabel("Name");
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            ImGui::InputText("##sdrpp_add_offset_name", newOffsetName, 1023);

            ImGui::LeftLabel("Offset");
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            ImGui::InputDouble("##sdrpp_add_offset_offset", &newOffset);

            bool denyApply = !newOffsetName[0] || offsets.nameExists(newOffsetName);

            if (denyApply) { style::beginDisabled(); }
            if (ImGui::Button("Apply")) {
                addOffset(newOffsetName, newOffset);
                open = false;
            }
            if (denyApply) { style::endDisabled(); }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                open = false;
            }
            ImGui::EndPopup();
        }
        return open;
    }

    void draw(void* ctx) {
        float itemWidth = ImGui::GetContentRegionAvail().x;
        float lineHeight = ImGui::GetTextLineHeightWithSpacing();
        float spacing = lineHeight - ImGui::GetTextLineHeight();
        bool running = gui::mainWindow.sdrIsRunning();

        if (running) { style::beginDisabled(); }

        ImGui::SetNextItemWidth(itemWidth);
        if (ImGui::Combo("##source", &sourceId, sources.txt)) {
            std::string newSource = sources.value(sourceId);
            selectSource(newSource);
            core::configManager.acquire();
            core::configManager.conf["source"] = newSource;
            core::configManager.release(true);
        }

        if (running) { style::endDisabled(); }

        sigpath::sourceManager.showSelectedMenu();

        if (ImGui::Checkbox("IQ Correction##_sdrpp_iq_corr", &iqCorrection)) {
            sigpath::iqFrontEnd.setDCBlocking(iqCorrection);
            core::configManager.acquire();
            core::configManager.conf["iqCorrection"] = iqCorrection;
            core::configManager.release(true);
        }

        if (ImGui::Checkbox("Invert IQ##_sdrpp_inv_iq", &invertIQ)) {
            sigpath::iqFrontEnd.setInvertIQ(invertIQ);
            core::configManager.acquire();
            core::configManager.conf["invertIQ"] = invertIQ;
            core::configManager.release(true);
        }

        ImGui::LeftLabel("Offset mode");
        ImGui::SetNextItemWidth(itemWidth - ImGui::GetCursorPosX() - 2.0f*(lineHeight + 1.5f*spacing));
        if (ImGui::Combo("##_sdrpp_offset_mode", &offsetMode, offsets.txt)) {
            updateOffset();
            core::configManager.acquire();
            core::configManager.conf["offsetMode"] = offsetMode;
            core::configManager.release(true);
        }
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - spacing);
        if (offsetMode < OFFSET_MODE_CUSTOM_BASE) { ImGui::BeginDisabled(); }
        if (ImGui::Button("-##_sdrpp_offset_del_", ImVec2(lineHeight + 0.5f*spacing, 0))) {
            delOffsetName = "TEST";
            showDelOffsetDialog = true;
        }
        if (offsetMode < OFFSET_MODE_CUSTOM_BASE) { ImGui::EndDisabled(); }
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - spacing);
        if (ImGui::Button("+##_sdrpp_offset_add_", ImVec2(lineHeight + 0.5f*spacing, 0))) {
            strcpy(newOffsetName, "New Offset");
            showAddOffsetDialog = true;
        }

        // Offset delete confirmation
        if (ImGui::GenericDialog("sdrpp_del_offset_confirm", showDelOffsetDialog, GENERIC_DIALOG_BUTTONS_YES_NO, []() {
            ImGui::Text("Deleting offset named \"%s\". Are you sure?", delOffsetName);
        }) == GENERIC_DIALOG_BUTTON_YES) {
            delOffset(delOffsetName);
        }

        // Offset add diaglog
        if (showAddOffsetDialog) { showAddOffsetDialog = addOffsetDialog(); }

        ImGui::LeftLabel("Offset");
        ImGui::FillWidth();
        if (offsetMode == OFFSET_MODE_MANUAL) {
            if (ImGui::InputDouble("##freq_offset", &customOffset, 1.0, 100.0)) {
                updateOffset();
                core::configManager.acquire();
                core::configManager.conf["offset"] = customOffset;
                core::configManager.release(true);
            }
        }
        else {
            style::beginDisabled();
            ImGui::InputDouble("##freq_offset", &effectiveOffset, 1.0, 100.0);
            style::endDisabled();
        }

        if (running) { style::beginDisabled(); }
        ImGui::LeftLabel("Decimation");
        ImGui::FillWidth();
        if (ImGui::Combo("##source_decim", &decimId, decimations.txt)) {
            sigpath::iqFrontEnd.setDecimation(decimations.value(decimId));
            core::configManager.acquire();
            core::configManager.conf["decimation"] = decimations.key(decimId);
            core::configManager.release(true);
        }
        if (running) { style::endDisabled(); }
    }
}
