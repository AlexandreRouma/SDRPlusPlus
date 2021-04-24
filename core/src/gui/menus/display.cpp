#include <gui/menus/display.h>
#include <imgui.h>
#include <gui/gui.h>
#include <core.h>
#include <gui/colormaps.h>
#include <gui/gui.h>
#include <gui/main_window.h>

namespace displaymenu {
    bool showWaterfall;
    bool fastFFT = true;
    bool fullWaterfallUpdate = true;
    int colorMapId = 0;
    std::vector<std::string> colorMapNames;
    std::string colorMapNamesTxt = "";
    std::string colorMapAuthor = "";

    const int FFTSizes[] = {
        65536,
        32768,
        16384,
        8192,
        4096,
        2048,
        1024
    };

    const char* FFTSizesStr = "65536\0"
                            "32768\0"
                            "16384\0"
                            "8192\0"
                            "4096\0"
                            "2048\0"
                            "1024\0";

    int fftSizeId = 0;

    void init() {
        showWaterfall = core::configManager.conf["showWaterfall"];
        showWaterfall ? gui::waterfall.showWaterfall() : gui::waterfall.hideWaterfall();
        std::string colormapName = core::configManager.conf["colorMap"];
        if (colormaps::maps.find(colormapName) != colormaps::maps.end()) {
            colormaps::Map map = colormaps::maps[colormapName];
            gui::waterfall.updatePalletteFromArray(map.map, map.entryCount);
        }

        for (auto const& [name, map] : colormaps::maps) {
            colorMapNames.push_back(name);
            colorMapNamesTxt += name;
            colorMapNamesTxt += '\0';
            if (name == colormapName) {
                colorMapId = (colorMapNames.size() - 1);
                colorMapAuthor = map.author;
            }
        }

        fastFFT = core::configManager.conf["fastFFT"];
        gui::waterfall.setFastFFT(fastFFT);

        fullWaterfallUpdate = core::configManager.conf["fullWaterfallUpdate"];
        gui::waterfall.setFullWaterfallUpdate(fullWaterfallUpdate);

        fftSizeId = 0;
        int fftSize = core::configManager.conf["fftSize"];
        for (int i = 0; i < 7; i++) {
            if (fftSize == FFTSizes[i]) {
                fftSizeId = i;
                break;
            }
        }
        setFFTSize(FFTSizes[fftSizeId]);

    }

    void draw(void* ctx) {
        float menuWidth = ImGui::GetContentRegionAvailWidth();
        if (ImGui::Checkbox("Show Waterfall##_sdrpp", &showWaterfall) || ImGui::IsKeyPressed(GLFW_KEY_HOME, false)) {
            if (ImGui::IsKeyPressed(GLFW_KEY_HOME, false)) { showWaterfall = !showWaterfall; }
            showWaterfall ? gui::waterfall.showWaterfall() : gui::waterfall.hideWaterfall();
            core::configManager.aquire();
            core::configManager.conf["showWaterfall"] = showWaterfall;
            core::configManager.release(true);
        }

        if (ImGui::Checkbox("Fast FFT##_sdrpp", &fastFFT)) {
            gui::waterfall.setFastFFT(fastFFT);
            core::configManager.aquire();
            core::configManager.conf["fastFFT"] = fastFFT;
            core::configManager.release(true);
        }

        if (ImGui::Checkbox("Full Waterfall Update##_sdrpp", &fullWaterfallUpdate)) {
            gui::waterfall.setFullWaterfallUpdate(fullWaterfallUpdate);
            core::configManager.aquire();
            core::configManager.conf["fullWaterfallUpdate"] = fullWaterfallUpdate;
            core::configManager.release(true);
        }

        ImGui::Text("FFT Size");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo("##test_fft_size", &fftSizeId, FFTSizesStr)) {
            setFFTSize(FFTSizes[fftSizeId]);
            core::configManager.aquire();
            core::configManager.conf["fftSize"] = FFTSizes[fftSizeId];
            core::configManager.release(true);
        }

        if (colorMapNames.size() > 0) {
            ImGui::Text("Color Map");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::Combo("##_sdrpp_color_map_sel", &colorMapId, colorMapNamesTxt.c_str())) {
                colormaps::Map map = colormaps::maps[colorMapNames[colorMapId]];
                gui::waterfall.updatePalletteFromArray(map.map, map.entryCount);
                core::configManager.aquire();
                core::configManager.conf["colorMap"] = colorMapNames[colorMapId];
                core::configManager.release(true);
                colorMapAuthor = map.author;
            }
            ImGui::Text("Color map Author: %s", colorMapAuthor.c_str());
        }

        
    }
}