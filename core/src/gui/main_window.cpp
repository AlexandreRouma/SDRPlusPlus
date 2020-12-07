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
#include <module.h>
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
#include <signal_path/source.h>
#include <gui/dialogs/loading_screen.h>

std::thread worker;
std::mutex fft_mtx;
fftwf_complex *fft_in, *fft_out;
fftwf_plan p;
float* tempData;
char buf[1024];

int fftSize = 8192 * 8;

std::vector<float> _data;
std::vector<float> fftTaps;
void fftHandler(dsp::complex_t* samples, int count, void* ctx) {
    if (count < fftSize) {
        return;
    }
    memcpy(fft_in, samples, count * sizeof(dsp::complex_t));
    fftwf_execute(p);
    int half = fftSize / 2;

    for (int i = 0; i < half; i++) {
        _data.push_back(log10(std::abs(std::complex<float>(fft_out[half + i][0], fft_out[half + i][1])) / (float)fftSize) * 10.0);
    }
    for (int i = 0; i < half; i++) {
        _data.push_back(log10(std::abs(std::complex<float>(fft_out[i][0], fft_out[i][1])) / (float)fftSize) * 10.0);
    }

    for (int i = 5; i < fftSize; i++) {
        _data[i] = (_data[i - 4]  + _data[i - 3] + _data[i - 2] + _data[i - 1] + _data[i]) / 5.0;
    }

    gui::waterfall.pushFFT(_data, fftSize);
    _data.clear();
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
dsp::stream<dsp::complex_t> dummyStream;

void windowInit() {
    LoadingScreen::show("Initializing UI");
    gui::waterfall.init();

    credits::init();

    core::configManager.aquire();
    gui::menu.order = core::configManager.conf["menuOrder"].get<std::vector<std::string>>();
    core::configManager.release();

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
    mod::loadFromList(ROOT_DIR "/module_list.json");

    sourecmenu::init();
    sinkmenu::init();
    scriptingmenu::init();
    bandplanmenu::init();
    displaymenu::init();

    // TODO for 0.2.5
    // Add "select folder" option for the file source
    // Add default main config to avoid having to ship one
    // Have a good directory system on both linux and windows (should fix occassional underruns)
    // Switch to double buffering

    // TODO for 0.2.6
    // Add a module add/remove/change order menu
    // Change the way fft samples are stored to make it less CPU intensive

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

    core::configManager.release();
}

void setVFO(double freq) {
    ImGui::WaterfallVFO* vfo = gui::waterfall.vfos[gui::waterfall.selectedVFO];

    double currentOff =  vfo->centerOffset;
    double currentTune = gui::waterfall.getCenterFrequency() + vfo->generalOffset;
    double delta = freq - currentTune;

    double newVFO = currentOff + delta;
    double vfoBW = vfo->bandwidth;
    double vfoBottom = newVFO - (vfoBW / 2.0);
    double vfoTop = newVFO + (vfoBW / 2.0);

    double view = gui::waterfall.getViewOffset();
    double viewBW = gui::waterfall.getViewBandwidth();
    double viewBottom = view - (viewBW / 2.0);
    double viewTop = view + (viewBW / 2.0);

    double wholeFreq = gui::waterfall.getCenterFrequency();
    double BW = gui::waterfall.getBandwidth();
    double bottom = -(BW / 2.0);
    double top = (BW / 2.0);

    // VFO still fints in the view
    if (vfoBottom > viewBottom && vfoTop < viewTop) {
        sigpath::vfoManager.setCenterOffset(gui::waterfall.selectedVFO, newVFO);
        return;
    }

    // VFO too low for current SDR tuning
    if (vfoBottom < bottom) {
        gui::waterfall.setViewOffset((BW / 2.0) - (viewBW / 2.0));
        double newVFOOffset = (BW / 2.0) - (vfoBW / 2.0) - (viewBW / 10.0);
        sigpath::vfoManager.setCenterOffset(gui::waterfall.selectedVFO, newVFOOffset);
        gui::waterfall.setCenterFrequency(freq - newVFOOffset);
        sigpath::sourceManager.tune(freq - newVFOOffset);
        return;
    }

    // VFO too high for current SDR tuning
    if (vfoTop > top) {
        gui::waterfall.setViewOffset((viewBW / 2.0) - (BW / 2.0));
        double newVFOOffset = (vfoBW / 2.0) - (BW / 2.0) + (viewBW / 10.0);
        sigpath::vfoManager.setCenterOffset(gui::waterfall.selectedVFO, newVFOOffset);
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

    ImGui::WaterfallVFO* vfo = gui::waterfall.vfos[gui::waterfall.selectedVFO];

    if (vfo->centerOffsetChanged) {
        gui::freqSelect.setFrequency(gui::waterfall.getCenterFrequency() + vfo->generalOffset);
        gui::freqSelect.frequencyChanged = false;
        core::configManager.aquire();
        core::configManager.conf["frequency"] = gui::freqSelect.frequency;
        core::configManager.release(true);
    }

    sigpath::vfoManager.updateFromWaterfall(&gui::waterfall);

    if (gui::waterfall.selectedVFOChanged) {
        gui::waterfall.selectedVFOChanged = false;
        gui::freqSelect.setFrequency(vfo->generalOffset + gui::waterfall.getCenterFrequency());
        gui::freqSelect.frequencyChanged = false;
        core::configManager.aquire();
        core::configManager.conf["frequency"] = gui::freqSelect.frequency;
        core::configManager.release(true);
    }

    if (gui::freqSelect.frequencyChanged) {
        gui::freqSelect.frequencyChanged = false;
        setVFO(gui::freqSelect.frequency);
        vfo->centerOffsetChanged = false;
        vfo->lowerOffsetChanged = false;
        vfo->upperOffsetChanged = false;
        core::configManager.aquire();
        core::configManager.conf["frequency"] = gui::freqSelect.frequency;
        core::configManager.release(true);
    }

    if (gui::waterfall.centerFreqMoved) {
        gui::waterfall.centerFreqMoved = false;
        sigpath::sourceManager.tune(gui::waterfall.getCenterFrequency());
        gui::freqSelect.setFrequency(gui::waterfall.getCenterFrequency() + vfo->generalOffset);
        core::configManager.aquire();
        core::configManager.conf["frequency"] = gui::freqSelect.frequency;
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
    sigpath::sinkManager.showVolumeSlider(gui::waterfall.selectedVFO, "##_sdrpp_main_volume_", 248, 30, 5);

    ImGui::SameLine();

    gui::freqSelect.draw();

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

        gui::menu.draw();

        if(ImGui::CollapsingHeader("Debug")) {
            ImGui::Text("Frame time: %.3f ms/frame", 1000.0 / ImGui::GetIO().Framerate);
            ImGui::Text("Framerate: %.1f FPS", ImGui::GetIO().Framerate);
            ImGui::Text("Center Frequency: %.0f Hz", gui::waterfall.getCenterFrequency());
            ImGui::Text("Source name: %s", sourceName.c_str());
            if (ImGui::Checkbox("Test technique", &dcbias.val)) {
                //sigpath::signalPath.setDCBiasCorrection(dcbias.val);
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
        gui::waterfall.setViewOffset(vfo->centerOffset); // center vfo on screen
    }

    ImGui::NewLine();

    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0) - (ImGui::CalcTextSize("Max").x / 2.0));
    ImGui::Text("Max");
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0) - 10);
    if (ImGui::VSliderFloat("##_8_", ImVec2(20.0, 150.0), &fftMax, 0.0, -100.0, "")) {
        fftMax = std::max<float>(fftMax, fftMin + 10);
        core::configManager.aquire();
        core::configManager.conf["max"] = fftMax;
        core::configManager.release(true);
    }

    ImGui::NewLine();

    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0) - (ImGui::CalcTextSize("Min").x / 2.0));
    ImGui::Text("Min");
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0) - 10);
    if (ImGui::VSliderFloat("##_9_", ImVec2(20.0, 150.0), &fftMin, 0.0, -100.0, "")) {
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
}

void setViewBandwidthSlider(float bandwidth) {
    bw = bandwidth;
}