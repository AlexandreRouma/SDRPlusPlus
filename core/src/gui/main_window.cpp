#include <gui/main_window.h>
#include <gui/gui.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <thread>
#include <complex>
#include <gui/widgets/waterfall.h>
#include <gui/widgets/frequency_select.h>
#include <signal_path/dsp.h>
#include <gui/icons.h>
#include <gui/widgets/bandplan.h>
#include <gui/style.h>
#include <config.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/menus/source.h>
#include <gui/menus/display.h>
#include <gui/menus/bandplan.h>
#include <gui/menus/sink.h>
#include <gui/menus/scripting.h>
#include <gui/menus/vfo_color.h>
#include <gui/menus/module_manager.h>
#include <gui/menus/theme.h>
#include <gui/dialogs/credits.h>
#include <filesystem>
#include <signal_path/source.h>
#include <gui/dialogs/loading_screen.h>
#include <options.h>
#include <gui/colormaps.h>
#include <gui/widgets/snr_meter.h>
#include <gui/tuner.h>

void MainWindow::init() {
    LoadingScreen::show("Initializing UI");
    gui::waterfall.init();
    gui::waterfall.setRawFFTSize(fftSize);

    appliedWindow = new float[fftSize];
    generateFFTWindow(selectedWindow, fftSize);

    credits::init();

    core::configManager.acquire();
    json menuElements = core::configManager.conf["menuElements"];
    std::string modulesDir = core::configManager.conf["modulesDirectory"];
    std::string resourcesDir = core::configManager.conf["resourcesDirectory"];
    core::configManager.release();

    // Load menu elements
    gui::menu.order.clear();
    for (auto& elem : menuElements) {
        if (!elem.contains("name")) { spdlog::error("Menu element is missing name key"); continue; }
        if (!elem["name"].is_string()) { spdlog::error("Menu element name isn't a string"); continue; }
        if (!elem.contains("open")) { spdlog::error("Menu element is missing open key"); continue; }
        if (!elem["open"].is_boolean()) { spdlog::error("Menu element name isn't a string"); continue; }
        Menu::MenuOption_t opt;
        opt.name = elem["name"];
        opt.open = elem["open"];
        gui::menu.order.push_back(opt);
    }

    gui::menu.registerEntry("Source", sourecmenu::draw, NULL);
    gui::menu.registerEntry("Sinks", sinkmenu::draw, NULL);
    gui::menu.registerEntry("Scripting", scriptingmenu::draw, NULL);
    gui::menu.registerEntry("Band Plan", bandplanmenu::draw, NULL);
    gui::menu.registerEntry("Display", displaymenu::draw, NULL);
    gui::menu.registerEntry("Theme", thememenu::draw, NULL);
    gui::menu.registerEntry("VFO Color", vfo_color_menu::draw, NULL);
    gui::menu.registerEntry("Module Manager", module_manager_menu::draw, NULL);
    
    gui::freqSelect.init();

    // Set default values for waterfall in case no source init's it
    gui::waterfall.setBandwidth(8000000);
    gui::waterfall.setViewBandwidth(8000000);
    
    fft_in = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * fftSize);
    fft_out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * fftSize);
    fftwPlan = fftwf_plan_dft_1d(fftSize, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);

    sigpath::signalPath.init(8000000, 20, fftSize, &dummyStream, (dsp::complex_t*)fft_in, fftHandler, this);
    sigpath::signalPath.start();

    vfoCreatedHandler.handler = vfoAddedHandler;
    vfoCreatedHandler.ctx = this;
    sigpath::vfoManager.vfoCreatedEvent.bindHandler(&vfoCreatedHandler);

    spdlog::info("Loading modules");

    // Load modules from /module directory
    if (std::filesystem::is_directory(modulesDir)) {
        for (const auto & file : std::filesystem::directory_iterator(modulesDir)) {
            std::string path = file.path().generic_string();
            if (file.path().extension().generic_string() != SDRPP_MOD_EXTENTSION) {
                continue;
            }
            if (!file.is_regular_file()) { continue; }
            spdlog::info("Loading {0}", path);
            LoadingScreen::show("Loading " + path);
            core::moduleManager.loadModule(path);
        }
    }
    else {
        spdlog::warn("Module directory {0} does not exist, not loading modules from directory", modulesDir);
    }

    // Read module config
    core::configManager.acquire();
    std::vector<std::string> modules = core::configManager.conf["modules"];
    auto modList = core::configManager.conf["moduleInstances"].items();
    core::configManager.release();

    // Load additional modules specified through config
    for (auto const& path : modules) {
        spdlog::info("Loading {0}", path);
        LoadingScreen::show("Loading " + path);
        core::moduleManager.loadModule(path);
    }

    // Create module instances
    for (auto const& [name, _module] : modList) {
        std::string mod = _module["module"];
        bool enabled = _module["enabled"];
        spdlog::info("Initializing {0} ({1})", name, mod);
        LoadingScreen::show("Initializing " + name + " (" + mod + ")");
        core::moduleManager.createInstance(name, mod);
        if (!enabled) { core::moduleManager.disableInstance(name); }
    }

    // Load color maps
    LoadingScreen::show("Loading color maps");
    spdlog::info("Loading color maps");
    if (std::filesystem::is_directory(resourcesDir + "/colormaps")) {
        for (const auto & file : std::filesystem::directory_iterator(resourcesDir + "/colormaps")) {
            std::string path = file.path().generic_string();
            LoadingScreen::show("Loading " + path);
            spdlog::info("Loading {0}", path);
            if (file.path().extension().generic_string() != ".json") {
                continue;
            }
            if (!file.is_regular_file()) { continue; }
            colormaps::loadMap(path);
        }
    }
    else {
        spdlog::warn("Color map directory {0} does not exist, not loading modules from directory", modulesDir);
    }

    gui::waterfall.updatePalletteFromArray(colormaps::maps["Turbo"].map, colormaps::maps["Turbo"].entryCount);

    sourecmenu::init();
    sinkmenu::init();
    scriptingmenu::init();
    bandplanmenu::init();
    displaymenu::init();
    vfo_color_menu::init();
    module_manager_menu::init();

    // TODO for 0.2.5
    // Fix gain not updated on startup, soapysdr

    // Update UI settings
    LoadingScreen::show("Loading configuration");
    core::configManager.acquire();
    fftMin = core::configManager.conf["min"];
    fftMax = core::configManager.conf["max"];
    gui::waterfall.setFFTMin(fftMin);
    gui::waterfall.setWaterfallMin(fftMin);
    gui::waterfall.setFFTMax(fftMax);
    gui::waterfall.setWaterfallMax(fftMax);

    double frequency = core::configManager.conf["frequency"];

    showMenu = core::configManager.conf["showMenu"];
    startedWithMenuClosed = !showMenu;

    gui::freqSelect.setFrequency(frequency);
    gui::freqSelect.frequencyChanged = false;
    sigpath::sourceManager.tune(frequency);
    gui::waterfall.setCenterFrequency(frequency);
    bw = gui::waterfall.getBandwidth();
    gui::waterfall.vfoFreqChanged = false;
    gui::waterfall.centerFreqMoved = false;
    gui::waterfall.selectFirstVFO();

    menuWidth = core::configManager.conf["menuWidth"];
    newWidth = menuWidth;

    fftHeight = core::configManager.conf["fftHeight"];
    gui::waterfall.setFFTHeight(fftHeight);

    tuningMode = core::configManager.conf["centerTuning"] ? tuner::TUNER_MODE_CENTER : tuner::TUNER_MODE_NORMAL;

    core::configManager.release();
}

void MainWindow::fftHandler(dsp::complex_t* samples, int count, void* ctx) {
    MainWindow* _this = (MainWindow*)ctx;
    std::lock_guard<std::mutex> lck(_this->fft_mtx);
    if (count != _this->fftSize) { return; }

    // Apply window
    volk_32fc_32f_multiply_32fc((lv_32fc_t*)_this->fft_in, (lv_32fc_t*)samples, _this->appliedWindow, count);

    // Execute FFT
    fftwf_execute(_this->fftwPlan);

    // Get the FFT buffer
    float* fftBuf = gui::waterfall.getFFTBuffer();
    if (fftBuf == NULL) {
        gui::waterfall.pushFFT();
        return;
    }

    // Take power of spectrum
    volk_32fc_s32f_power_spectrum_32f(fftBuf, (lv_32fc_t*)_this->fft_out, count, count);

    // Push back data
    gui::waterfall.pushFFT();
}

void MainWindow::vfoAddedHandler(VFOManager::VFO* vfo, void* ctx) {
    MainWindow* _this = (MainWindow*)ctx;
    std::string name = vfo->getName();
    core::configManager.acquire();
    if (!core::configManager.conf["vfoOffsets"].contains(name)) {
        core::configManager.release();
        return;
    }
    double offset = core::configManager.conf["vfoOffsets"][name];
    core::configManager.release();
    sigpath::vfoManager.setOffset(name, std::clamp<double>(offset, -_this->bw/2.0, _this->bw/2.0));
}

void MainWindow::draw() {
    ImGui::Begin("Main", NULL, WINDOW_FLAGS);

    ImGui::WaterfallVFO* vfo = NULL;
    if (gui::waterfall.selectedVFO != "") {
        vfo = gui::waterfall.vfos[gui::waterfall.selectedVFO];
    }

    // Handle VFO movement
    if (vfo != NULL) {
        if (vfo->centerOffsetChanged) {
            if (tuningMode == tuner::TUNER_MODE_CENTER) {
                tuner::tune(tuner::TUNER_MODE_CENTER, gui::waterfall.selectedVFO, gui::waterfall.getCenterFrequency() + vfo->generalOffset);
            }
            gui::freqSelect.setFrequency(gui::waterfall.getCenterFrequency() + vfo->generalOffset);
            gui::freqSelect.frequencyChanged = false;
            core::configManager.acquire();
            core::configManager.conf["vfoOffsets"][gui::waterfall.selectedVFO] = vfo->generalOffset;
            core::configManager.release(true);
        }
    }
    
    sigpath::vfoManager.updateFromWaterfall(&gui::waterfall);

    // Handle selection of another VFO
    if (gui::waterfall.selectedVFOChanged) {
        gui::freqSelect.setFrequency((vfo != NULL) ? (vfo->generalOffset + gui::waterfall.getCenterFrequency()) : gui::waterfall.getCenterFrequency());
        gui::waterfall.selectedVFOChanged = false;
        gui::freqSelect.frequencyChanged = false;
    }

    // Handle change in selected frequency
    if (gui::freqSelect.frequencyChanged) {
        gui::freqSelect.frequencyChanged = false;
        tuner::tune(tuningMode, gui::waterfall.selectedVFO, gui::freqSelect.frequency);
        if (vfo != NULL) {
            vfo->centerOffsetChanged = false;
            vfo->lowerOffsetChanged = false;
            vfo->upperOffsetChanged = false;
        }
        core::configManager.acquire();
        core::configManager.conf["frequency"] = gui::waterfall.getCenterFrequency();
        if (vfo != NULL) {
            core::configManager.conf["vfoOffsets"][gui::waterfall.selectedVFO] = vfo->generalOffset;
        }
        core::configManager.release(true);
    }

    // Handle dragging the frequency scale
    if (gui::waterfall.centerFreqMoved) {
        gui::waterfall.centerFreqMoved = false;
        sigpath::sourceManager.tune(gui::waterfall.getCenterFrequency());
        if (vfo != NULL) {
            gui::freqSelect.setFrequency(gui::waterfall.getCenterFrequency() + vfo->generalOffset);
        }
        else {
            gui::freqSelect.setFrequency(gui::waterfall.getCenterFrequency());
        }
        core::configManager.acquire();
        core::configManager.conf["frequency"] = gui::waterfall.getCenterFrequency();
        core::configManager.release(true);
    }

    int _fftHeight = gui::waterfall.getFFTHeight();
    if (fftHeight != _fftHeight) {
        fftHeight = _fftHeight;
        core::configManager.acquire();
        core::configManager.conf["fftHeight"] = fftHeight;
        core::configManager.release(true);
    }

    ImVec2 vMin = ImGui::GetWindowContentRegionMin();
    ImVec2 vMax = ImGui::GetWindowContentRegionMax();

    int width = vMax.x - vMin.x;
    int height = vMax.y - vMin.y;

    // To Bar
    ImGui::PushID(ImGui::GetID("sdrpp_menu_btn"));
    if (ImGui::ImageButton(icons::MENU, ImVec2(30, 30), ImVec2(0, 0), ImVec2(1, 1), 5) || ImGui::IsKeyPressed(GLFW_KEY_MENU, false)) {
        showMenu = !showMenu;
        core::configManager.acquire();
        core::configManager.conf["showMenu"] = showMenu;
        core::configManager.release(true);
    }
    ImGui::PopID();

    ImGui::SameLine();

    if (playing) {
        ImGui::PushID(ImGui::GetID("sdrpp_stop_btn"));
        if (ImGui::ImageButton(icons::STOP, ImVec2(30, 30), ImVec2(0, 0), ImVec2(1, 1), 5) || ImGui::IsKeyPressed(GLFW_KEY_END, false)) {
            sigpath::sourceManager.stop();
            playing = false;
        }
        ImGui::PopID();
    }
    else { // TODO: Might need to check if there even is a device
        ImGui::PushID(ImGui::GetID("sdrpp_play_btn"));
        if (ImGui::ImageButton(icons::PLAY, ImVec2(30, 30), ImVec2(0, 0), ImVec2(1, 1), 5) || ImGui::IsKeyPressed(GLFW_KEY_END, false)) {
            sigpath::sourceManager.start();
            // TODO: tune in module instead
            sigpath::sourceManager.tune(gui::waterfall.getCenterFrequency());
            playing = true;
        }
        ImGui::PopID();
    }

    ImGui::SameLine();

    //ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
    sigpath::sinkManager.showVolumeSlider(gui::waterfall.selectedVFO, "##_sdrpp_main_volume_", 248, 30, 5, true);

    ImGui::SameLine();

    gui::freqSelect.draw();

    ImGui::SameLine();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - 9);
    if (tuningMode == tuner::TUNER_MODE_CENTER) {
        ImGui::PushID(ImGui::GetID("sdrpp_ena_st_btn"));
        if (ImGui::ImageButton(icons::CENTER_TUNING, ImVec2(30, 30), ImVec2(0, 0), ImVec2(1, 1), 5)) {
            tuningMode = tuner::TUNER_MODE_NORMAL;
            core::configManager.acquire();
            core::configManager.conf["centerTuning"] = false;
            core::configManager.release(true);
        }
        ImGui::PopID();
    }
    else { // TODO: Might need to check if there even is a device
        ImGui::PushID(ImGui::GetID("sdrpp_dis_st_btn"));
        if (ImGui::ImageButton(icons::NORMAL_TUNING, ImVec2(30, 30), ImVec2(0, 0), ImVec2(1, 1), 5)) {
            tuningMode = tuner::TUNER_MODE_CENTER;
            tuner::tune(tuner::TUNER_MODE_CENTER, gui::waterfall.selectedVFO, gui::freqSelect.frequency);
            core::configManager.acquire();
            core::configManager.conf["centerTuning"] = true;
            core::configManager.release(true);
        }
        ImGui::PopID();
    }

    ImGui::SameLine();

    ImGui::SetCursorPosX(ImGui::GetWindowSize().x - 387);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
    ImGui::SetNextItemWidth(300);
    ImGui::SNRMeter((vfo != NULL) ? gui::waterfall.selectedVFOSNR : 0);

    ImGui::SameLine();

    // Logo button
    ImGui::SetCursorPosX(ImGui::GetWindowSize().x - 48);
    ImGui::SetCursorPosY(10);
    if (ImGui::ImageButton(icons::LOGO, ImVec2(32, 32), ImVec2(0, 0), ImVec2(1, 1), 0)) {
        showCredits = true;
    }
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        showCredits = false;
    }
    if (ImGui::IsKeyPressed(GLFW_KEY_ESCAPE)) {
        showCredits = false;
    }

    // Handle menu resize
    ImVec2 winSize = ImGui::GetWindowSize();
    ImVec2 mousePos = ImGui::GetMousePos();
    if (!lockWaterfallControls) {
        float curY = ImGui::GetCursorPosY();
        bool click = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        bool down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        if (grabbingMenu) {
            newWidth = mousePos.x;
            newWidth = std::clamp<float>(newWidth, 250, winSize.x - 250);
            ImGui::GetForegroundDrawList()->AddLine(ImVec2(newWidth, curY), ImVec2(newWidth, winSize.y - 10), ImGui::GetColorU32(ImGuiCol_SeparatorActive));
        }
        if (mousePos.x >= newWidth - 2 && mousePos.x <= newWidth + 2 && mousePos.y > curY) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            if (click) {
                grabbingMenu = true;
            }
        }
        else {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
        }
        if(!down && grabbingMenu) {
            grabbingMenu = false;
            menuWidth = newWidth;
            core::configManager.acquire();
            core::configManager.conf["menuWidth"] = menuWidth;
            core::configManager.release(true);
        }
    }


    // Left Column
    lockWaterfallControls = false;
    if (showMenu) {
        ImGui::Columns(3, "WindowColumns", false);
        ImGui::SetColumnWidth(0, menuWidth);
        ImGui::SetColumnWidth(1, winSize.x - menuWidth - 60);
        ImGui::SetColumnWidth(2, 60);
        ImGui::BeginChild("Left Column");
        float menuColumnWidth = ImGui::GetContentRegionAvailWidth();

        if (gui::menu.draw(firstMenuRender)) {
            core::configManager.acquire();
            json arr = json::array();
            for (int i = 0; i < gui::menu.order.size(); i++) {
                arr[i]["name"] = gui::menu.order[i].name;
                arr[i]["open"] = gui::menu.order[i].open;
            }
            core::configManager.conf["menuElements"] = arr;

            // Update enabled and disabled modules
            for (auto [_name, inst] : core::moduleManager.instances) {
                if (!core::configManager.conf["moduleInstances"].contains(_name)) { continue; }
                core::configManager.conf["moduleInstances"][_name]["enabled"] = inst.instance->isEnabled();
            }

            core::configManager.release(true);
        }
        if (startedWithMenuClosed) {
            startedWithMenuClosed = false;  
        }
        else {
            firstMenuRender = false;
        }

        if(ImGui::CollapsingHeader("Debug")) {
            ImGui::Text("Frame time: %.3f ms/frame", 1000.0 / ImGui::GetIO().Framerate);
            ImGui::Text("Framerate: %.1f FPS", ImGui::GetIO().Framerate);
            ImGui::Text("Center Frequency: %.0f Hz", gui::waterfall.getCenterFrequency());
            ImGui::Text("Source name: %s", sourceName.c_str());
            ImGui::Checkbox("Show demo window", &demoWindow);
            ImGui::Text("ImGui version: %s", ImGui::GetVersion());

            ImGui::Checkbox("Bypass buffering", &sigpath::signalPath.inputBuffer.bypass);

            ImGui::Text("Buffering: %d", (sigpath::signalPath.inputBuffer.writeCur - sigpath::signalPath.inputBuffer.readCur + 32) % 32);
            
            if (ImGui::Button("Test Bug")) {
                spdlog::error("Will this make the software crash?");
            }

            if (ImGui::Button("Testing something")) {
                gui::menu.order[0].open = true;
                firstMenuRender = true;
            }

            ImGui::Spacing();
        }

        ImGui::EndChild();
    }
    else {
        // When hiding the menu bar
        ImGui::Columns(3, "WindowColumns", false);
        ImGui::SetColumnWidth(0, 8);
        ImGui::SetColumnWidth(1, winSize.x - 8 - 60);
        ImGui::SetColumnWidth(2, 60);
    }

    // Right Column
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::NextColumn();
    ImGui::PopStyleVar();

    ImGui::BeginChild("Waterfall");

    gui::waterfall.draw();    

    ImGui::EndChild();

    if (!lockWaterfallControls) {
        // Handle arrow keys
        if (vfo != NULL && (gui::waterfall.mouseInFFT || gui::waterfall.mouseInWaterfall)) {
            if (ImGui::IsKeyPressed(GLFW_KEY_LEFT) && !gui::freqSelect.digitHovered) {
                double nfreq = gui::waterfall.getCenterFrequency() + vfo->generalOffset - vfo->snapInterval;
                nfreq = roundl(nfreq / vfo->snapInterval) * vfo->snapInterval;
                tuner::tune(tuningMode, gui::waterfall.selectedVFO, nfreq);
            }
            if (ImGui::IsKeyPressed(GLFW_KEY_RIGHT) && !gui::freqSelect.digitHovered) {
                double nfreq = gui::waterfall.getCenterFrequency() + vfo->generalOffset + vfo->snapInterval;
                nfreq = roundl(nfreq / vfo->snapInterval) * vfo->snapInterval;
                tuner::tune(tuningMode, gui::waterfall.selectedVFO, nfreq);
            }
            core::configManager.acquire();
            core::configManager.conf["frequency"] = gui::waterfall.getCenterFrequency();
            if (vfo != NULL) {
                core::configManager.conf["vfoOffsets"][gui::waterfall.selectedVFO] = vfo->generalOffset;
            }
            core::configManager.release(true);
        }

        // Handle scrollwheel
        int wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0 && (gui::waterfall.mouseInFFT || gui::waterfall.mouseInWaterfall)) {
            double nfreq;
            if (vfo != NULL) {
                nfreq = gui::waterfall.getCenterFrequency() + vfo->generalOffset + (vfo->snapInterval * wheel);
                nfreq = roundl(nfreq / vfo->snapInterval) * vfo->snapInterval;
            }
            else {
                nfreq = gui::waterfall.getCenterFrequency() - (gui::waterfall.getViewBandwidth() * wheel / 20.0);
            }
            tuner::tune(tuningMode, gui::waterfall.selectedVFO, nfreq);
            gui::freqSelect.setFrequency(nfreq);
            core::configManager.acquire();
            core::configManager.conf["frequency"] = gui::waterfall.getCenterFrequency();
            if (vfo != NULL) {
                core::configManager.conf["vfoOffsets"][gui::waterfall.selectedVFO] = vfo->generalOffset;
            }
            core::configManager.release(true);
        }
    }
    
    ImGui::NextColumn();
    ImGui::BeginChild("WaterfallControls");

    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0) - (ImGui::CalcTextSize("Zoom").x / 2.0));
    ImGui::Text("Zoom");
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0) - 10);
    if (ImGui::VSliderFloat("##_7_", ImVec2(20.0, 150.0), &bw, gui::waterfall.getBandwidth(), 1000.0, "")) {
        gui::waterfall.setViewBandwidth(bw);
        if (vfo != NULL) {
            gui::waterfall.setViewOffset(vfo->centerOffset); // center vfo on screen
        }
    }

    ImGui::NewLine();

    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0) - (ImGui::CalcTextSize("Max").x / 2.0));
    ImGui::Text("Max");
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0) - 10);
    if (ImGui::VSliderFloat("##_8_", ImVec2(20.0, 150.0), &fftMax, 0.0, -160.0f, "")) {
        fftMax = std::max<float>(fftMax, fftMin + 10);
        core::configManager.acquire();
        core::configManager.conf["max"] = fftMax;
        core::configManager.release(true);
    }

    ImGui::NewLine();

    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0) - (ImGui::CalcTextSize("Min").x / 2.0));
    ImGui::Text("Min");
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0) - 10);
    if (ImGui::VSliderFloat("##_9_", ImVec2(20.0, 150.0), &fftMin, 0.0, -160.0f, "")) {
        fftMin = std::min<float>(fftMax - 10, fftMin);
        core::configManager.acquire();
        core::configManager.conf["min"] = fftMin;
        core::configManager.release(true);
    }

    ImGui::EndChild();

    gui::waterfall.setFFTMin(fftMin);
    gui::waterfall.setFFTMax(fftMax);
    gui::waterfall.setWaterfallMin(fftMin);
    gui::waterfall.setWaterfallMax(fftMax);

    ImGui::End();

    if (showCredits) {
        credits::show();
    }

    if (demoWindow) {
        ImGui::ShowDemoWindow();
    }
}

void MainWindow::setViewBandwidthSlider(float bandwidth) {
    bw = bandwidth;
}

bool MainWindow::sdrIsRunning() {
    return playing;
}

void MainWindow::setFFTSize(int size) {
    std::lock_guard<std::mutex> lck(fft_mtx);
    fftSize = size;

    gui::waterfall.setRawFFTSize(fftSize);
    sigpath::signalPath.setFFTSize(fftSize);

    fftwf_free(fft_in);
    fftwf_free(fft_out);
    fftwf_destroy_plan(fftwPlan);
    
    fft_in = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * fftSize);
    fft_out = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * fftSize);
    fftwPlan = fftwf_plan_dft_1d(fftSize, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);

    delete appliedWindow;

    appliedWindow = new float[fftSize];
    generateFFTWindow(selectedWindow, fftSize);
}

void MainWindow::setFFTWindow(int win) {
    std::lock_guard<std::mutex> lck(fft_mtx);
    selectedWindow = win;
    generateFFTWindow(selectedWindow, fftSize);
}

void MainWindow::generateFFTWindow(int win, int size) {
    if (win == FFT_WINDOW_RECTANGULAR) {
        for (int i = 0; i < size; i++) {
            appliedWindow[i] = (i%2) ? 1 : -1;
        }
    }
    else if (win == FFT_WINDOW_BLACKMAN) {
        for (int i = 0; i < size; i++) {
            appliedWindow[i] = ((i%2) ? dsp::window_function::blackman(i, size) : -dsp::window_function::blackman(i, size))*2;
        }
    }
}

bool MainWindow::isPlaying() {
    return playing;
}