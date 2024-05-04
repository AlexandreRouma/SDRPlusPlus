#include <gui/menus/display.h>
#include <imgui.h>
#include <gui/gui.h>
#include <core.h>
#include <gui/colormaps.h>
#include <gui/gui.h>
#include <gui/main_window.h>
#include <signal_path/signal_path.h>
#include <gui/style.h>
#include <utils/optionlist.h>
#include <algorithm>

namespace displaymenu {
    bool showWaterfall;
    bool fullWaterfallUpdate = true;
    int colorMapId = 0;
    std::vector<std::string> colorMapNames;
    std::string colorMapNamesTxt = "";
    std::string colorMapAuthor = "";
    int selectedWindow = 0;
    int fftRate = 20;
    int fftSizeId = 0;
    int uiScaleId = 0;
    bool restartRequired = false;
    bool fftHold = false;
    int fftHoldSpeed = 60;
    bool fftSmoothing = false;
    int fftSmoothingSpeed = 100;
    bool snrSmoothing = false;
    int snrSmoothingSpeed = 20;

    OptionList<int, int> fftSizes;
    OptionList<float, float> uiScales;

    const IQFrontEnd::FFTWindow fftWindowList[] = {
        IQFrontEnd::FFTWindow::RECTANGULAR,
        IQFrontEnd::FFTWindow::BLACKMAN,
        IQFrontEnd::FFTWindow::NUTTALL
    };

    void updateFFTSpeeds() {
        gui::waterfall.setFFTHoldSpeed((float)fftHoldSpeed / ((float)fftRate * 10.0f));
        gui::waterfall.setFFTSmoothingSpeed(std::min<float>((float)fftSmoothingSpeed / (float)(fftRate * 10.0f), 1.0f));
        gui::waterfall.setSNRSmoothingSpeed(std::min<float>((float)snrSmoothingSpeed / (float)(fftRate * 10.0f), 1.0f));
    }

    void init() {
        // Define FFT sizes
        fftSizes.define(524288, "524288", 524288);
        fftSizes.define(262144, "262144", 262144);
        fftSizes.define(131072, "131072", 131072);
        fftSizes.define(65536, "65536", 65536);
        fftSizes.define(32768, "32768", 32768);
        fftSizes.define(16384, "16384", 16384);
        fftSizes.define(8192, "8192", 8192);
        fftSizes.define(4096, "4096", 4096);
        fftSizes.define(2048, "2048", 2048);
        fftSizes.define(1024, "1024", 1024);

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

        fullWaterfallUpdate = core::configManager.conf["fullWaterfallUpdate"];
        gui::waterfall.setFullWaterfallUpdate(fullWaterfallUpdate);

        fftSizeId = fftSizes.valueId(65536);
        int size = core::configManager.conf["fftSize"];
        if (fftSizes.keyExists(size)) {
            fftSizeId = fftSizes.keyId(size);
        }
        sigpath::iqFrontEnd.setFFTSize(fftSizes.value(fftSizeId));

        fftRate = core::configManager.conf["fftRate"];
        sigpath::iqFrontEnd.setFFTRate(fftRate);

        selectedWindow = std::clamp<int>((int)core::configManager.conf["fftWindow"], 0, (sizeof(fftWindowList) / sizeof(IQFrontEnd::FFTWindow)) - 1);
        sigpath::iqFrontEnd.setFFTWindow(fftWindowList[selectedWindow]);

        gui::menu.locked = core::configManager.conf["lockMenuOrder"];

        fftHold = core::configManager.conf["fftHold"];
        fftHoldSpeed = core::configManager.conf["fftHoldSpeed"];
        gui::waterfall.setFFTHold(fftHold);
        fftSmoothing = core::configManager.conf["fftSmoothing"];
        fftSmoothingSpeed = core::configManager.conf["fftSmoothingSpeed"];
        gui::waterfall.setFFTSmoothing(fftSmoothing);
        snrSmoothing = core::configManager.conf["snrSmoothing"];
        snrSmoothingSpeed = core::configManager.conf["snrSmoothingSpeed"];
        gui::waterfall.setSNRSmoothing(snrSmoothing);
        updateFFTSpeeds();

        // Define and load UI scales
        uiScales.define(1.0f, "100%", 1.0f);
        uiScales.define(2.0f, "200%", 2.0f);
        uiScales.define(3.0f, "300%", 3.0f);
        uiScales.define(4.0f, "400%", 4.0f);
        uiScaleId = uiScales.valueId(style::uiScale);
    }

    void setWaterfallShown(bool shown) {
        showWaterfall = shown;
        showWaterfall ? gui::waterfall.showWaterfall() : gui::waterfall.hideWaterfall();
        core::configManager.acquire();
        core::configManager.conf["showWaterfall"] = showWaterfall;
        core::configManager.release(true);
    }

    void checkKeybinds() {
        if (ImGui::IsKeyPressed(ImGuiKey_Home, false)) {
            setWaterfallShown(!showWaterfall);
        }
    }

    void draw(void* ctx) {
        float menuWidth = ImGui::GetContentRegionAvail().x;
        if (ImGui::Checkbox("Show Waterfall##_sdrpp", &showWaterfall)) {
            setWaterfallShown(showWaterfall);
        }

        if (ImGui::Checkbox("Full Waterfall Update##_sdrpp", &fullWaterfallUpdate)) {
            gui::waterfall.setFullWaterfallUpdate(fullWaterfallUpdate);
            core::configManager.acquire();
            core::configManager.conf["fullWaterfallUpdate"] = fullWaterfallUpdate;
            core::configManager.release(true);
        }

        if (ImGui::Checkbox("Lock Menu Order##_sdrpp", &gui::menu.locked)) {
            core::configManager.acquire();
            core::configManager.conf["lockMenuOrder"] = gui::menu.locked;
            core::configManager.release(true);
        }

        if (ImGui::Checkbox("FFT Hold##_sdrpp", &fftHold)) {
            gui::waterfall.setFFTHold(fftHold);
            core::configManager.acquire();
            core::configManager.conf["fftHold"] = fftHold;
            core::configManager.release(true);
        }
        ImGui::SameLine();
        ImGui::FillWidth();
        if (ImGui::InputInt("##sdrpp_fft_hold_speed", &fftHoldSpeed)) {
            updateFFTSpeeds();
            core::configManager.acquire();
            core::configManager.conf["fftHoldSpeed"] = fftHoldSpeed;
            core::configManager.release(true);
        }

        if (ImGui::Checkbox("FFT Smoothing##_sdrpp", &fftSmoothing)) {
            gui::waterfall.setFFTSmoothing(fftSmoothing);
            core::configManager.acquire();
            core::configManager.conf["fftSmoothing"] = fftSmoothing;
            core::configManager.release(true);
        }
        ImGui::SameLine();
        ImGui::FillWidth();
        if (ImGui::InputInt("##sdrpp_fft_smoothing_speed", &fftSmoothingSpeed)) {
            fftSmoothingSpeed = std::max<int>(fftSmoothingSpeed, 1);
            updateFFTSpeeds();
            core::configManager.acquire();
            core::configManager.conf["fftSmoothingSpeed"] = fftSmoothingSpeed;
            core::configManager.release(true);
        }

        if (ImGui::Checkbox("SNR Smoothing##_sdrpp", &snrSmoothing)) {
            gui::waterfall.setSNRSmoothing(snrSmoothing);
            core::configManager.acquire();
            core::configManager.conf["snrSmoothing"] = snrSmoothing;
            core::configManager.release(true);
        }
        ImGui::SameLine();
        ImGui::FillWidth();
        if (ImGui::InputInt("##sdrpp_snr_smoothing_speed", &snrSmoothingSpeed)) {
            snrSmoothingSpeed = std::max<int>(snrSmoothingSpeed, 1);
            updateFFTSpeeds();
            core::configManager.acquire();
            core::configManager.conf["snrSmoothingSpeed"] = snrSmoothingSpeed;
            core::configManager.release(true);
        }

        ImGui::LeftLabel("High-DPI Scaling");
        ImGui::FillWidth();
        if (ImGui::Combo("##sdrpp_ui_scale", &uiScaleId, uiScales.txt)) {
            core::configManager.acquire();
            core::configManager.conf["uiScale"] = uiScales[uiScaleId];
            core::configManager.release(true);
            restartRequired = true;
        }

        ImGui::LeftLabel("FFT Framerate");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputInt("##sdrpp_fft_rate", &fftRate, 1, 10)) {
            fftRate = std::max<int>(1, fftRate);
            sigpath::iqFrontEnd.setFFTRate(fftRate);
            updateFFTSpeeds();
            core::configManager.acquire();
            core::configManager.conf["fftRate"] = fftRate;
            core::configManager.release(true);
        }

        ImGui::LeftLabel("FFT Size");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo("##sdrpp_fft_size", &fftSizeId, fftSizes.txt)) {
            sigpath::iqFrontEnd.setFFTSize(fftSizes.value(fftSizeId));
            core::configManager.acquire();
            core::configManager.conf["fftSize"] = fftSizes.key(fftSizeId);
            core::configManager.release(true);
        }

        ImGui::LeftLabel("FFT Window");
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::Combo("##sdrpp_fft_window", &selectedWindow, "Rectangular\0Blackman\0Nuttall\0")) {
            sigpath::iqFrontEnd.setFFTWindow(fftWindowList[selectedWindow]);
            core::configManager.acquire();
            core::configManager.conf["fftWindow"] = selectedWindow;
            core::configManager.release(true);
        }

        if (colorMapNames.size() > 0) {
            ImGui::LeftLabel("Color Map");
            ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
            if (ImGui::Combo("##_sdrpp_color_map_sel", &colorMapId, colorMapNamesTxt.c_str())) {
                colormaps::Map map = colormaps::maps[colorMapNames[colorMapId]];
                gui::waterfall.updatePalletteFromArray(map.map, map.entryCount);
                core::configManager.acquire();
                core::configManager.conf["colorMap"] = colorMapNames[colorMapId];
                core::configManager.release(true);
                colorMapAuthor = map.author;
            }
            ImGui::Text("Color map Author: %s", colorMapAuthor.c_str());
        }

        if (restartRequired) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Restart required.");
        }
    }
}
