#include <gui/main_window.h>
#include <gui/gui.h>

std::thread worker;
std::mutex fft_mtx;
fftwf_complex *fft_in, *fft_out;
fftwf_plan p;
float* tempData;
char buf[1024];

int fftSize = 8192 * 8;

std::vector<float> _data;
std::vector<float> fftTaps;
void fftHandler(dsp::complex_t* samples) {
    fftwf_execute(p);
    int half = fftSize / 2;

    for (int i = 0; i < half; i++) {
        _data.push_back(log10(std::abs(std::complex<float>(fft_out[half + i][0], fft_out[half + i][1])) / (float)fftSize) * 10.0f);
    }
    for (int i = 0; i < half; i++) {
        _data.push_back(log10(std::abs(std::complex<float>(fft_out[i][0], fft_out[i][1])) / (float)fftSize) * 10.0f);
    }

    for (int i = 5; i < fftSize; i++) {
        _data[i] = (_data[i - 4]  + _data[i - 3] + _data[i - 2] + _data[i - 1] + _data[i]) / 5.0f;
    }

    gui::waterfall.pushFFT(_data, fftSize);
    _data.clear();
}

dsp::NullSink sink;
watcher<uint64_t> freq((uint64_t)90500000);
watcher<float> vfoFreq(92000000.0f);
float dummyVolume = 1.0f;
float* volume = &dummyVolume;
float fftMin = -70.0f;
float fftMax = 0.0f;
watcher<float> offset(0.0f, true);
watcher<float> bw(8000000.0f, true);
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
    spdlog::info("Initializing SoapySDR");

    credits::init();

    gui::menu.registerEntry("Source", sourecmenu::draw, NULL);
    gui::menu.registerEntry("Audio", audiomenu::draw, NULL);
    gui::menu.registerEntry("Band Plan", bandplanmenu::draw, NULL);
    gui::menu.registerEntry("Display", displaymenu::draw, NULL);
    
    gui::freqSelect.init();
    
    fft_in = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * fftSize);
    fft_out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * fftSize);
    p = fftwf_plan_dft_1d(fftSize, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);

    sigpath::signalPath.init(8000000, 20, fftSize, &dummyStream, (dsp::complex_t*)fft_in, fftHandler);
    sigpath::signalPath.start();

    spdlog::info("Loading modules");
    mod::loadFromList(ROOT_DIR "/module_list.json");

    sourecmenu::init();
    audiomenu::init();
    bandplanmenu::init();
    displaymenu::init();

    // Load last source configuration

    // Also add a loading screen
    // Adjustable "snap to grid" for each VFO
    // Finish the recorder module
    // Add squelsh
    // Bandwidth ajustment
    // CW and RAW modes;
    // Bring VFO to a visible place when changing sample rate if it's smaller
    // Add save config for modules
    // Do VFO in two steps: First sample rate conversion, then filtering

    // And a module add/remove/change order menu
    // get rid of watchers and use if() instead
    // Switch to double for all frequecies and bandwidth

    // Update UI settings
    fftMin = core::configManager.conf["min"];
    fftMax = core::configManager.conf["max"];
    gui::waterfall.setFFTMin(fftMin);
    gui::waterfall.setWaterfallMin(fftMin);
    gui::waterfall.setFFTMax(fftMax);
    gui::waterfall.setWaterfallMax(fftMax);

    float frequency = core::configManager.conf["frequency"];
    

    gui::freqSelect.setFrequency(frequency);
    gui::freqSelect.frequencyChanged = false;
    sigpath::sourceManager.tune(frequency);
    gui::waterfall.setCenterFrequency(frequency);
    gui::waterfall.setBandwidth(8000000);
    gui::waterfall.setViewBandwidth(8000000);
    bw.val = 8000000;
    gui::waterfall.vfoFreqChanged = false;
    gui::waterfall.centerFreqMoved = false;
    gui::waterfall.selectFirstVFO();

    menuWidth = core::configManager.conf["menuWidth"];
    newWidth = menuWidth;

    fftHeight = core::configManager.conf["fftHeight"];
    gui::waterfall.setFFTHeight(fftHeight);

    core::configManager.release();
}

void setVFO(float freq) {
    ImGui::WaterfallVFO* vfo = gui::waterfall.vfos[gui::waterfall.selectedVFO];

    float currentOff =  vfo->centerOffset;
    float currentTune = gui::waterfall.getCenterFrequency() + vfo->generalOffset;
    float delta = freq - currentTune;

    float newVFO = currentOff + delta;
    float vfoBW = vfo->bandwidth;
    float vfoBottom = newVFO - (vfoBW / 2.0f);
    float vfoTop = newVFO + (vfoBW / 2.0f);

    float view = gui::waterfall.getViewOffset();
    float viewBW = gui::waterfall.getViewBandwidth();
    float viewBottom = view - (viewBW / 2.0f);
    float viewTop = view + (viewBW / 2.0f);

    float wholeFreq = gui::waterfall.getCenterFrequency();
    float BW = gui::waterfall.getBandwidth();
    float bottom = -(BW / 2.0f);
    float top = (BW / 2.0f);

    // VFO still fints in the view
    if (vfoBottom > viewBottom && vfoTop < viewTop) {
        sigpath::vfoManager.setCenterOffset(gui::waterfall.selectedVFO, newVFO);
        return;
    }

    // VFO too low for current SDR tuning
    if (vfoBottom < bottom) {
        gui::waterfall.setViewOffset((BW / 2.0f) - (viewBW / 2.0f));
        float newVFOOffset = (BW / 2.0f) - (vfoBW / 2.0f) - (viewBW / 10.0f);
        sigpath::vfoManager.setCenterOffset(gui::waterfall.selectedVFO, newVFOOffset);
        gui::waterfall.setCenterFrequency(freq - newVFOOffset);
        sigpath::sourceManager.tune(freq - newVFOOffset);
        return;
    }

    // VFO too high for current SDR tuning
    if (vfoTop > top) {
        gui::waterfall.setViewOffset((viewBW / 2.0f) - (BW / 2.0f));
        float newVFOOffset = (vfoBW / 2.0f) - (BW / 2.0f) + (viewBW / 10.0f);
        sigpath::vfoManager.setCenterOffset(gui::waterfall.selectedVFO, newVFOOffset);
        gui::waterfall.setCenterFrequency(freq - newVFOOffset);
        sigpath::sourceManager.tune(freq - newVFOOffset);
        return;
    }

    // VFO is still without the SDR's bandwidth
    if (delta < 0) {
        float newViewOff = vfoTop - (viewBW / 2.0f) + (viewBW / 10.0f);
        float newViewBottom = newViewOff - (viewBW / 2.0f);
        float newViewTop = newViewOff + (viewBW / 2.0f);

        if (newViewBottom > bottom) {
            gui::waterfall.setViewOffset(newViewOff);
            sigpath::vfoManager.setCenterOffset(gui::waterfall.selectedVFO, newVFO);
            return;
        }

        gui::waterfall.setViewOffset((BW / 2.0f) - (viewBW / 2.0f));
        float newVFOOffset = (BW / 2.0f) - (vfoBW / 2.0f) - (viewBW / 10.0f);
        sigpath::vfoManager.setCenterOffset(gui::waterfall.selectedVFO, newVFOOffset);
        gui::waterfall.setCenterFrequency(freq - newVFOOffset);
        sigpath::sourceManager.tune(freq - newVFOOffset);
    }
    else {
        float newViewOff = vfoBottom + (viewBW / 2.0f) - (viewBW / 10.0f);
        float newViewBottom = newViewOff - (viewBW / 2.0f);
        float newViewTop = newViewOff + (viewBW / 2.0f);

        if (newViewTop < top) {
            gui::waterfall.setViewOffset(newViewOff);
            sigpath::vfoManager.setCenterOffset(gui::waterfall.selectedVFO, newVFO);
            return;
        }

        gui::waterfall.setViewOffset((viewBW / 2.0f) - (BW / 2.0f));
        float newVFOOffset = (vfoBW / 2.0f) - (BW / 2.0f) + (viewBW / 10.0f);
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
        audioStreamName = audio::getNameFromVFO(gui::waterfall.selectedVFO);
        if (audioStreamName != "") {
            volume = &audio::streams[audioStreamName]->volume;
        }
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

    if (dcbias.changed()) {
        sigpath::signalPath.setDCBiasCorrection(dcbias.val);
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
    if (ImGui::ImageButton(icons::MENU, ImVec2(40, 40), ImVec2(0, 0), ImVec2(1, 1), 0)) {
        showMenu = !showMenu;
    }

    ImGui::SameLine();

    if (playing) {
        if (ImGui::ImageButton(icons::STOP, ImVec2(40, 40), ImVec2(0, 0), ImVec2(1, 1), 0)) {
            sigpath::sourceManager.stop();
            playing = false;
        }
    }
    else { // TODO: Might need to check if there even is a device
        if (ImGui::ImageButton(icons::PLAY, ImVec2(40, 40), ImVec2(0, 0), ImVec2(1, 1), 0)) {
            sigpath::sourceManager.start();
            // TODO: tune in module instead
            sigpath::sourceManager.tune(gui::waterfall.getCenterFrequency());
            playing = true;
        }
    }

    ImGui::SameLine();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
    ImGui::SetNextItemWidth(200);
    if (ImGui::SliderFloat("##_2_", volume, 0.0f, 1.0f, "")) {
        if (audioStreamName != "") {
            core::configManager.aquire();
            if (!core::configManager.conf["audio"].contains(audioStreamName)) {
                //saveAudioConfig(audioStreamName);
                // TODO: FIX THIS SHIT
            }
            audio::streams[audioStreamName]->audio->setVolume(*volume);
            core::configManager.conf["audio"][audioStreamName]["volume"] = *volume;
            core::configManager.release(true);
        }
    }

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
            ImGui::Text("Frame time: %.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);
            ImGui::Text("Framerate: %.1f FPS", ImGui::GetIO().Framerate);
            ImGui::Text("Center Frequency: %.0f Hz", gui::waterfall.getCenterFrequency());
            ImGui::Text("Source name: %s", sourceName.c_str());
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

    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0f) - (ImGui::CalcTextSize("Zoom").x / 2.0f));
    ImGui::Text("Zoom");
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0f) - 10);
    // TODO: use global sample rate value from DSP instead of 8000000
    ImGui::VSliderFloat("##_7_", ImVec2(20.0f, 150.0f), &bw.val, 8000000, 1000.0f, "");

    ImGui::NewLine();

    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0f) - (ImGui::CalcTextSize("Max").x / 2.0f));
    ImGui::Text("Max");
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0f) - 10);
    if (ImGui::VSliderFloat("##_8_", ImVec2(20.0f, 150.0f), &fftMax, 0.0f, -100.0f, "")) {
        fftMax = std::max<float>(fftMax, fftMin + 10);
        core::configManager.aquire();
        core::configManager.conf["max"] = fftMax;
        core::configManager.release(true);
    }

    ImGui::NewLine();

    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0f) - (ImGui::CalcTextSize("Min").x / 2.0f));
    ImGui::Text("Min");
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0f) - 10);
    if (ImGui::VSliderFloat("##_9_", ImVec2(20.0f, 150.0f), &fftMin, 0.0f, -100.0f, "")) {
        fftMin = std::min<float>(fftMax - 10, fftMin);
        core::configManager.aquire();
        core::configManager.conf["min"] = fftMin;
        core::configManager.release(true);
    }

    ImGui::EndChild();

    if (bw.changed()) {
        gui::waterfall.setViewBandwidth(bw.val);
        gui::waterfall.setViewOffset(vfo->centerOffset);
    }

    gui::waterfall.setFFTMin(fftMin);
    gui::waterfall.setFFTMax(fftMax);
    gui::waterfall.setWaterfallMin(fftMin);
    gui::waterfall.setWaterfallMax(fftMax);


    ImGui::End();

    if (showCredits) {
        credits::show();
    }
}

void bindVolumeVariable(float* vol) {
    volume = vol;
}

void unbindVolumeVariable() {
    volume = &dummyVolume;
}