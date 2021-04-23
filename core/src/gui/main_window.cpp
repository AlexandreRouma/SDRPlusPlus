#include <gui/main_window.h>
#include <gui/gui.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <imgui_plot.h>
#include <thread>
#include <complex>
#include <gui/widgets/waterfall.h>
#include <gui/widgets/frequency_select.h>
#include <fftw3.h>
#include <signal_path/dsp.h>
#include <gui/icons.h>
#include <gui/widgets/bandplan.h>
#include <watcher.h>
#include <signal_path/vfo_manager.h>
#include <gui/style.h>
#include <config.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/menus/source.h>
#include <gui/menus/display.h>
#include <gui/menus/bandplan.h>
#include <gui/menus/sink.h>
#include <gui/menus/scripting.h>
#include <gui/dialogs/credits.h>
#include <filesystem>
#include <signal_path/source.h>
#include <gui/dialogs/loading_screen.h>
#include <options.h>
#include <gui/colormaps.h>

int fftSize = 8192 * 8;

std::mutex fft_mtx;
fftwf_complex *fft_in, *fft_out;
fftwf_plan p;
float* tempFFT;
float* FFTdata;
char buf[1024];
bool experimentalZoom = false;
bool firstMenuRender = true;
bool startedWithMenuClosed = false;

void fftHandler(dsp::complex_t* samples, int count, void* ctx) {
    std::lock_guard<std::mutex> lck(fft_mtx);
    if (count != fftSize) { return; }

    memcpy(fft_in, samples, count * sizeof(dsp::complex_t));
    fftwf_execute(p);
    int half = count / 2;

    // volk_32fc_s32f_power_spectrum_32f(FFTdata, (lv_32fc_t*)fft_out, count, count);

    // memcpy(tempFFT, &FFTdata[half], half * sizeof(float));
    // memmove(&FFTdata[half], FFTdata, half * sizeof(float));
    // memcpy(FFTdata, tempFFT, half * sizeof(float));

    // float* fftBuf = gui::waterfall.getFFTBuffer();
    // if (fftBuf == NULL) {
    //     gui::waterfall.pushFFT();
    //     return;
    // }

    // memcpy(fftBuf, FFTdata, count * sizeof(float));

    // gui::waterfall.pushFFT();

    float* fftBuf = gui::waterfall.getFFTBuffer();
    if (fftBuf == NULL) {
        gui::waterfall.pushFFT();
        return;
    }

    volk_32fc_s32f_power_spectrum_32f(tempFFT, (lv_32fc_t*)fft_out, count, count);

    memcpy(fftBuf, &tempFFT[half], half * sizeof(float));
    memcpy(&fftBuf[half], tempFFT, half * sizeof(float));

    gui::waterfall.pushFFT();
}

watcher<uint64_t> freq((uint64_t)90500000);
watcher<double> vfoFreq(92000000.0);
float fftMin = -70.0;
float fftMax = 0.0;
watcher<double> offset(0.0, true);
float bw = 8000000;
bool playing = false;
watcher<bool> dcbias(false, false);
bool showCredits = false;
std::string audioStreamName = "";
std::string sourceName = "";
int menuWidth = 300;
bool grabbingMenu = false;
int newWidth = 300;
int fftHeight = 300;
bool showMenu = true;
bool centerTuning = false;
dsp::stream<dsp::complex_t> dummyStream;
bool demoWindow = false;

void windowInit() {
    LoadingScreen::show("Initializing UI");
    gui::waterfall.init();
    gui::waterfall.setRawFFTSize(fftSize);

    tempFFT = new float[fftSize];
    FFTdata = new float[fftSize];

    credits::init();

    core::configManager.aquire();
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
    
    gui::freqSelect.init();

    // Set default values for waterfall in case no source init's it
    gui::waterfall.setBandwidth(8000000);
    gui::waterfall.setViewBandwidth(8000000);
    
    fft_in = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * fftSize);
    fft_out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * fftSize);
    p = fftwf_plan_dft_1d(fftSize, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);

    sigpath::signalPath.init(8000000, 20, fftSize, &dummyStream, (dsp::complex_t*)fft_in, fftHandler);
    sigpath::signalPath.start();

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
    core::configManager.aquire();
    std::vector<std::string> modules = core::configManager.conf["modules"];
    std::map<std::string, std::string> modList = core::configManager.conf["moduleInstances"];
    core::configManager.release();

    // Load additional modules specified through config
    for (auto const& path : modules) {
        spdlog::info("Loading {0}", path);
        LoadingScreen::show("Loading " + path);
        core::moduleManager.loadModule(path);
    }

    // Create module instances
    for (auto const& [name, module] : modList) {
        spdlog::info("Initializing {0} ({1})", name, module);
        LoadingScreen::show("Initializing " + name + " (" + module + ")");
        core::moduleManager.createInstance(name, module);
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

    // TODO for 0.2.5
    // Add "select file" option for the file source
    // Have a good directory system on both linux and windows
    // Switch to double buffering (should fix occassional underruns)
    // Fix gain not updated on startup, soapysdr

    // TODO for 0.2.6
    // Add a module add/remove/change order menu

    // Update UI settings
    LoadingScreen::show("Loading configuration");
    core::configManager.aquire();
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

    centerTuning = core::configManager.conf["centerTuning"];

    // Load each VFO's offset
    for (auto const& [name, _vfo] : gui::waterfall.vfos) {
        if (!core::configManager.conf["vfoOffsets"].contains(name)) {
            continue;
        }
        double offset = core::configManager.conf["vfoOffsets"][name];
        sigpath::vfoManager.setOffset(name, std::clamp<double>(offset, -bw/2.0, bw/2.0));
    }

    core::configManager.release();
}

void setVFO(double freq) {
    double viewBW = gui::waterfall.getViewBandwidth();
    double BW = gui::waterfall.getBandwidth();
    if (gui::waterfall.selectedVFO == "") {
        gui::waterfall.setViewOffset((BW / 2.0) - (viewBW / 2.0));
        gui::waterfall.setCenterFrequency(freq);
        sigpath::sourceManager.tune(freq);
        return;
    }

    ImGui::WaterfallVFO* vfo = gui::waterfall.vfos[gui::waterfall.selectedVFO];

    double currentOff =  vfo->centerOffset;
    double currentTune = gui::waterfall.getCenterFrequency() + vfo->generalOffset;
    double delta = freq - currentTune;

    double newVFO = currentOff + delta;
    double vfoBW = vfo->bandwidth;
    double vfoBottom = newVFO - (vfoBW / 2.0);
    double vfoTop = newVFO + (vfoBW / 2.0);

    double view = gui::waterfall.getViewOffset();
    double viewBottom = view - (viewBW / 2.0);
    double viewTop = view + (viewBW / 2.0);

    double wholeFreq = gui::waterfall.getCenterFrequency();
    double bottom = -(BW / 2.0);
    double top = (BW / 2.0);

    if (centerTuning) {
        gui::waterfall.setViewOffset((BW / 2.0) - (viewBW / 2.0));
        gui::waterfall.setCenterFrequency(freq);
        gui::waterfall.setViewOffset(0);
        sigpath::vfoManager.setOffset(gui::waterfall.selectedVFO, 0);
        sigpath::sourceManager.tune(freq);
        return;
    }

    // VFO still fints in the view
    if (vfoBottom > viewBottom && vfoTop < viewTop) {
        sigpath::vfoManager.setCenterOffset(gui::waterfall.selectedVFO, newVFO);
        return;
    }

    // VFO too low for current SDR tuning
    if (vfoBottom < bottom) {
        gui::waterfall.setViewOffset((BW / 2.0) - (viewBW / 2.0));
        double newVFOOffset = (BW / 2.0) - (vfoBW / 2.0) - (viewBW / 10.0);
        sigpath::vfoManager.setOffset(gui::waterfall.selectedVFO, newVFOOffset);
        gui::waterfall.setCenterFrequency(freq - newVFOOffset);
        sigpath::sourceManager.tune(freq - newVFOOffset);
        return;
    }

    // VFO too high for current SDR tuning
    if (vfoTop > top) {
        gui::waterfall.setViewOffset((viewBW / 2.0) - (BW / 2.0));
        double newVFOOffset = (vfoBW / 2.0) - (BW / 2.0) + (viewBW / 10.0);
        sigpath::vfoManager.setOffset(gui::waterfall.selectedVFO, newVFOOffset);
        gui::waterfall.setCenterFrequency(freq - newVFOOffset);
        sigpath::sourceManager.tune(freq - newVFOOffset);
        return;
    }

    // VFO is still without the SDR's bandwidth
    if (delta < 0) {
        double newViewOff = vfoTop - (viewBW / 2.0) + (viewBW / 10.0);
        double newViewBottom = newViewOff - (viewBW / 2.0);
        double newViewTop = newViewOff + (viewBW / 2.0);

        if (newViewBottom > bottom) {
            gui::waterfall.setViewOffset(newViewOff);
            sigpath::vfoManager.setCenterOffset(gui::waterfall.selectedVFO, newVFO);
            return;
        }

        gui::waterfall.setViewOffset((BW / 2.0) - (viewBW / 2.0));
        double newVFOOffset = (BW / 2.0) - (vfoBW / 2.0) - (viewBW / 10.0);
        sigpath::vfoManager.setCenterOffset(gui::waterfall.selectedVFO, newVFOOffset);
        gui::waterfall.setCenterFrequency(freq - newVFOOffset);
        sigpath::sourceManager.tune(freq - newVFOOffset);
    }
    else {
        double newViewOff = vfoBottom + (viewBW / 2.0) - (viewBW / 10.0);
        double newViewBottom = newViewOff - (viewBW / 2.0);
        double newViewTop = newViewOff + (viewBW / 2.0);

        if (newViewTop < top) {
            gui::waterfall.setViewOffset(newViewOff);
            sigpath::vfoManager.setCenterOffset(gui::waterfall.selectedVFO, newVFO);
            return;
        }

        gui::waterfall.setViewOffset((viewBW / 2.0) - (BW / 2.0));
        double newVFOOffset = (vfoBW / 2.0) - (BW / 2.0) + (viewBW / 10.0);
        sigpath::vfoManager.setCenterOffset(gui::waterfall.selectedVFO, newVFOOffset);
        gui::waterfall.setCenterFrequency(freq - newVFOOffset);
        sigpath::sourceManager.tune(freq - newVFOOffset);
    }
}

void drawWindow() {
    ImGui::Begin("Main", NULL, WINDOW_FLAGS);

    ImGui::WaterfallVFO* vfo = NULL;
    if (gui::waterfall.selectedVFO != "") {
        vfo = gui::waterfall.vfos[gui::waterfall.selectedVFO];
    }

    // Handke VFO movement
    if (vfo != NULL) {
        if (vfo->centerOffsetChanged) {
            if (centerTuning) {
                setVFO(gui::waterfall.getCenterFrequency() + vfo->generalOffset);
            }
            gui::freqSelect.setFrequency(gui::waterfall.getCenterFrequency() + vfo->generalOffset);
            gui::freqSelect.frequencyChanged = false;
            core::configManager.aquire();
            core::configManager.conf["vfoOffsets"][gui::waterfall.selectedVFO] = vfo->generalOffset;
            core::configManager.release(true);
        }
    }
    
    sigpath::vfoManager.updateFromWaterfall(&gui::waterfall);

    // Handle selection of another VFO
    if (gui::waterfall.selectedVFOChanged && vfo != NULL) {
        gui::waterfall.selectedVFOChanged = false;
        gui::freqSelect.setFrequency(vfo->generalOffset + gui::waterfall.getCenterFrequency());
        gui::freqSelect.frequencyChanged = false;
    }

    // Handle change in selected frequency
    if (gui::freqSelect.frequencyChanged) {
        gui::freqSelect.frequencyChanged = false;
        setVFO(gui::freqSelect.frequency);
        if (vfo != NULL) {
            vfo->centerOffsetChanged = false;
            vfo->lowerOffsetChanged = false;
            vfo->upperOffsetChanged = false;
        }
        core::configManager.aquire();
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
        core::configManager.aquire();
        core::configManager.conf["frequency"] = gui::waterfall.getCenterFrequency();
        core::configManager.release(true);
    }

    // Handle arrow keys
    if (vfo != NULL) {
        if (ImGui::IsKeyPressed(GLFW_KEY_LEFT) && !gui::freqSelect.digitHovered) {
            setVFO(gui::waterfall.getCenterFrequency() + vfo->generalOffset - vfo->snapInterval);
        }
        if (ImGui::IsKeyPressed(GLFW_KEY_RIGHT) && !gui::freqSelect.digitHovered) {
            setVFO(gui::waterfall.getCenterFrequency() + vfo->generalOffset + vfo->snapInterval);
        }
        core::configManager.aquire();
        core::configManager.conf["frequency"] = gui::waterfall.getCenterFrequency();
        if (vfo != NULL) {
            core::configManager.conf["vfoOffsets"][gui::waterfall.selectedVFO] = vfo->generalOffset;
        }
        core::configManager.release(true);
    }

    int _fftHeight = gui::waterfall.getFFTHeight();
    if (fftHeight != _fftHeight) {
        fftHeight = _fftHeight;
        core::configManager.aquire();
        core::configManager.conf["fftHeight"] = fftHeight;
        core::configManager.release(true);
    }

    ImVec2 vMin = ImGui::GetWindowContentRegionMin();
    ImVec2 vMax = ImGui::GetWindowContentRegionMax();

    int width = vMax.x - vMin.x;
    int height = vMax.y - vMin.y;

    // To Bar
    ImGui::PushID(ImGui::GetID("sdrpp_menu_btn"));
    if (ImGui::ImageButton(icons::MENU, ImVec2(30, 30), ImVec2(0, 0), ImVec2(1, 1), 5)) {
        showMenu = !showMenu;
        core::configManager.aquire();
        core::configManager.conf["showMenu"] = showMenu;
        core::configManager.release(true);
    }
    ImGui::PopID();

    ImGui::SameLine();

    if (playing) {
        ImGui::PushID(ImGui::GetID("sdrpp_stop_btn"));
        if (ImGui::ImageButton(icons::STOP, ImVec2(30, 30), ImVec2(0, 0), ImVec2(1, 1), 5)) {
            sigpath::sourceManager.stop();
            playing = false;
        }
        ImGui::PopID();
    }
    else { // TODO: Might need to check if there even is a device
        ImGui::PushID(ImGui::GetID("sdrpp_play_btn"));
        if (ImGui::ImageButton(icons::PLAY, ImVec2(30, 30), ImVec2(0, 0), ImVec2(1, 1), 5)) {
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
    if (centerTuning) {
        ImGui::PushID(ImGui::GetID("sdrpp_ena_st_btn"));
        if (ImGui::ImageButton(icons::CENTER_TUNING, ImVec2(30, 30), ImVec2(0, 0), ImVec2(1, 1), 5)) {
            centerTuning = false;
            core::configManager.aquire();
            core::configManager.conf["centerTuning"] = centerTuning;
            core::configManager.release(true);
        }
        ImGui::PopID();
    }
    else { // TODO: Might need to check if there even is a device
        ImGui::PushID(ImGui::GetID("sdrpp_dis_st_btn"));
        if (ImGui::ImageButton(icons::NORMAL_TUNING, ImVec2(30, 30), ImVec2(0, 0), ImVec2(1, 1), 5)) {
            centerTuning = true;
            setVFO(gui::freqSelect.frequency);
            core::configManager.aquire();
            core::configManager.conf["centerTuning"] = centerTuning;
            core::configManager.release(true);
        }
        ImGui::PopID();
    }

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
    float curY = ImGui::GetCursorPosY();
    ImVec2 winSize = ImGui::GetWindowSize();
    ImVec2 mousePos = ImGui::GetMousePos();
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
        core::configManager.aquire();
        core::configManager.conf["menuWidth"] = menuWidth;
        core::configManager.release(true);
    }

    // Left Column

    if (showMenu) {
        ImGui::Columns(3, "WindowColumns", false);
        ImGui::SetColumnWidth(0, menuWidth);
        ImGui::SetColumnWidth(1, winSize.x - menuWidth - 60);
        ImGui::SetColumnWidth(2, 60);
        ImGui::BeginChild("Left Column");
        float menuColumnWidth = ImGui::GetContentRegionAvailWidth();

        if (gui::menu.draw(firstMenuRender)) {
            core::configManager.aquire();
            json arr = json::array();
            for (int i = 0; i < gui::menu.order.size(); i++) {
                arr[i]["name"] = gui::menu.order[i].name;
                arr[i]["open"] = gui::menu.order[i].open;
            }
            core::configManager.conf["menuElements"] = arr;
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
            if (ImGui::Checkbox("Test technique", &dcbias.val)) {
                //sigpath::signalPath.setDCBiasCorrection(dcbias.val);
            }
            ImGui::Checkbox("Show demo window", &demoWindow);
            ImGui::Checkbox("Experimental zoom", &experimentalZoom);
            ImGui::Text("ImGui version: %s", ImGui::GetVersion());

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
        core::configManager.aquire();
        core::configManager.conf["max"] = fftMax;
        core::configManager.release(true);
    }

    ImGui::NewLine();

    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0) - (ImGui::CalcTextSize("Min").x / 2.0));
    ImGui::Text("Min");
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0) - 10);
    if (ImGui::VSliderFloat("##_9_", ImVec2(20.0, 150.0), &fftMin, 0.0, -160.0f, "")) {
        fftMin = std::min<float>(fftMax - 10, fftMin);
        core::configManager.aquire();
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

void setViewBandwidthSlider(float bandwidth) {
    bw = bandwidth;
}

bool sdrIsRunning() {
    return playing;
}

void setFFTSize(int size) {
    std::lock_guard<std::mutex> lck(fft_mtx);
    fftSize = size;

    gui::waterfall.setRawFFTSize(fftSize);
    sigpath::signalPath.setFFTSize(fftSize);

    fftwf_free(fft_in);
    fftwf_free(fft_out);
    fftwf_destroy_plan(p);
    
    fft_in = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * fftSize);
    fft_out = (fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex) * fftSize);
    p = fftwf_plan_dft_1d(fftSize, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);
}