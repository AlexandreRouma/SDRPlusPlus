#include <gui/main_window.h>
#include <gui/gui.h>

std::thread worker;
std::mutex fft_mtx;
fftwf_complex *fft_in, *fft_out;
fftwf_plan p;
float* tempData;
float* uiGains;
char buf[1024];
ImFont* bigFont;

int fftSize = 8192 * 8;

io::SoapyWrapper soapy;
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
int devId = 0;
int srId = 0;
watcher<int> bandplanId(0, true);
watcher<uint64_t> freq((uint64_t)90500000);
int demod = 1;
watcher<float> vfoFreq(92000000.0f);
float dummyVolume = 1.0f;
float* volume = &dummyVolume;
float fftMin = -70.0f;
float fftMax = 0.0f;
watcher<float> offset(0.0f, true);
watcher<float> bw(8000000.0f, true);
int sampleRate = 8000000;
bool playing = false;
watcher<bool> dcbias(false, false);
watcher<bool> bandPlanEnabled(true, false);
bool showCredits = false;
std::string audioStreamName = "";
std::string sourceName = "";
int menuWidth = 300;
bool grabbingMenu = false;
int newWidth = 300;
bool showWaterfall = true;
int fftHeight = 300;
bool showMenu = true;

void saveCurrentSource() {
    int i = 0;
    for (std::string gainName : soapy.gainList) {
        core::configManager.conf["sourceSettings"][sourceName]["gains"][gainName] = uiGains[i];
        i++;
    }
    core::configManager.conf["sourceSettings"][sourceName]["sampleRate"] = soapy.sampleRates[srId];
}

void loadSourceConfig(std::string name) {
    json sourceSettings = core::configManager.conf["sourceSettings"][name];

    sampleRate = sourceSettings["sampleRate"];
    
    auto _srIt = std::find(soapy.sampleRates.begin(), soapy.sampleRates.end(), sampleRate);
    // If the sample rate isn't valid, set to minimum
    if (_srIt == soapy.sampleRates.end()) {
        srId = 0;
        sampleRate = soapy.sampleRates[0];
    }
    else {
        srId = std::distance(soapy.sampleRates.begin(), _srIt);
    }
    sigpath::signalPath.setSampleRate(sampleRate);
    soapy.setSampleRate(sampleRate);

    // Set gains
    delete[] uiGains;
    uiGains = new float[soapy.gainList.size()];
    int i = 0;
    for (std::string gainName : soapy.gainList) {
        // If gain doesn't exist in config, set it to the minimum value
        if (!sourceSettings["gains"].contains(gainName)) {
            uiGains[i] = soapy.gainRanges[i].minimum();
            soapy.setGain(i, uiGains[i]);
            continue;
        }
        uiGains[i] = sourceSettings["gains"][gainName];
        soapy.setGain(i, uiGains[i]);
        i++;
    }

    // Update GUI
    gui::waterfall.setBandwidth(sampleRate);
    gui::waterfall.setViewBandwidth(sampleRate);
    bw.val = sampleRate;
}

void loadAudioConfig(std::string name) {
    json audioSettings = core::configManager.conf["audio"][name];
    std::string devName = audioSettings["device"];
    auto _devIt = std::find(audio::streams[name]->audio->deviceNames.begin(), audio::streams[name]->audio->deviceNames.end(), devName);

    // If audio device doesn't exist anymore
    if (_devIt == audio::streams[name]->audio->deviceNames.end()) {
        audio::streams[name]->audio->setToDefault();
        int deviceId = audio::streams[name]->audio->getDeviceId();
        audio::setAudioDevice(name, deviceId, audio::streams[name]->audio->devices[deviceId].sampleRates[0]);
        audio::streams[name]->sampleRateId = 0;
        audio::streams[name]->volume = audioSettings["volume"];
        audio::streams[name]->audio->setVolume(audio::streams[name]->volume);
        return;
    }
    int deviceId = std::distance(audio::streams[name]->audio->deviceNames.begin(), _devIt);
    float sr = audioSettings["sampleRate"];
    auto _srIt = std::find(audio::streams[name]->audio->devices[deviceId].sampleRates.begin(), audio::streams[name]->audio->devices[deviceId].sampleRates.end(), sr);

    // If sample rate doesn't exist anymore
    if (_srIt == audio::streams[name]->audio->devices[deviceId].sampleRates.end()) {
        audio::streams[name]->sampleRateId = 0;
        audio::setAudioDevice(name, deviceId, audio::streams[name]->audio->devices[deviceId].sampleRates[0]);
        audio::streams[name]->volume = audioSettings["volume"];
        audio::streams[name]->audio->setVolume(audio::streams[name]->volume);
        return;
    }

    int samplerateId = std::distance(audio::streams[name]->audio->devices[deviceId].sampleRates.begin(), _srIt);
    audio::streams[name]->sampleRateId = samplerateId;
    audio::setAudioDevice(name, deviceId, audio::streams[name]->audio->devices[deviceId].sampleRates[samplerateId]);
    audio::streams[name]->deviceId = deviceId;
    audio::streams[name]->volume = audioSettings["volume"];
    audio::streams[name]->audio->setVolume(audio::streams[name]->volume);
}

void saveAudioConfig(std::string name) {
    core::configManager.conf["audio"][name]["device"] = audio::streams[name]->audio->deviceNames[audio::streams[name]->deviceId];
    core::configManager.conf["audio"][name]["volume"] = audio::streams[name]->volume;
    core::configManager.conf["audio"][name]["sampleRate"] = audio::streams[name]->sampleRate;
}

void setVFO(float freq);

// =======================================================

void sourceMenu(void* ctx) {
    float menuColumnWidth = ImGui::GetContentRegionAvailWidth();
    if (playing) { style::beginDisabled(); };

    ImGui::PushItemWidth(menuColumnWidth);
    if (ImGui::Combo("##_0_", &devId, soapy.txtDevList.c_str())) {
        spdlog::info("Changed input device: {0}", devId);
        sourceName = soapy.devNameList[devId];
        soapy.setDevice(soapy.devList[devId]);

        core::configManager.aquire();
        if (core::configManager.conf["sourceSettings"].contains(sourceName)) {
            loadSourceConfig(sourceName);
        }
        else {
            srId = 0;
            sampleRate = soapy.getSampleRate();
            bw.val = sampleRate;
            gui::waterfall.setBandwidth(sampleRate);
            gui::waterfall.setViewBandwidth(sampleRate);
            sigpath::signalPath.setSampleRate(sampleRate);

            if (soapy.gainList.size() >= 0) {
                delete[] uiGains;
                uiGains = new float[soapy.gainList.size()];
                for (int i = 0; i < soapy.gainList.size(); i++) {
                    uiGains[i] = soapy.currentGains[i];
                }
            }
        }
        setVFO(gui::freqSelect.frequency);
        core::configManager.conf["source"] = sourceName;
        core::configManager.release(true);
    }
    ImGui::PopItemWidth();

    if (ImGui::Combo("##_1_", &srId, soapy.txtSampleRateList.c_str())) {
        spdlog::info("Changed sample rate: {0}", srId);
        sampleRate = soapy.sampleRates[srId];
        soapy.setSampleRate(sampleRate);
        gui::waterfall.setBandwidth(sampleRate);
        gui::waterfall.setViewBandwidth(sampleRate);
        sigpath::signalPath.setSampleRate(sampleRate);
        bw.val = sampleRate;

        core::configManager.aquire();
        if (!core::configManager.conf["sourceSettings"].contains(sourceName)) {
            saveCurrentSource();
        }
        core::configManager.conf["sourceSettings"][sourceName]["sampleRate"] = sampleRate;
        core::configManager.release(true);
    }

    ImGui::SameLine();
    bool noDevice = (soapy.devList.size() == 0);
    if (ImGui::Button("Refresh", ImVec2(menuColumnWidth - ImGui::GetCursorPosX(), 0.0f))) {
        soapy.refresh();
        if (noDevice && soapy.devList.size() > 0) {
            sourceName = soapy.devNameList[0];
            soapy.setDevice(soapy.devList[0]);
            if (core::configManager.conf["sourceSettings"][sourceName]) {
                loadSourceConfig(sourceName);
            }
        }
    }

    if (playing) { style::endDisabled(); };

    float maxTextLength = 0;
    float txtLen = 0;
    char buf[100];

    // Calculate the spacing
    for (int i = 0; i < soapy.gainList.size(); i++) {
        sprintf(buf, "%s gain", soapy.gainList[i].c_str());
        txtLen = ImGui::CalcTextSize(buf).x;
        if (txtLen > maxTextLength) {
            maxTextLength = txtLen;
        }
    }

    for (int i = 0; i < soapy.gainList.size(); i++) {
        ImGui::Text("%s gain", soapy.gainList[i].c_str());
        ImGui::SameLine();
        sprintf(buf, "##_gain_slide_%d_", i);

        ImGui::SetCursorPosX(maxTextLength + 5);
        ImGui::PushItemWidth(menuColumnWidth - (maxTextLength + 5));
        if (ImGui::SliderFloat(buf, &uiGains[i], soapy.gainRanges[i].minimum(), soapy.gainRanges[i].maximum())) {
            soapy.setGain(i, uiGains[i]);
            core::configManager.aquire();
            if (!core::configManager.conf["sourceSettings"].contains(sourceName)) {
                saveCurrentSource();
            }
            core::configManager.conf["sourceSettings"][sourceName]["gains"][soapy.gainList[i]] = uiGains[i];
            core::configManager.release(true);
        }
        ImGui::PopItemWidth();
    }

    ImGui::Spacing();
}

// =======================================================

void windowInit() {
    spdlog::info("Initializing SoapySDR");
    soapy.init();

    gui::menu.registerEntry("Source", sourceMenu, NULL);
    
    gui::freqSelect.init();
    
    fft_in = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * fftSize);
    fft_out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * fftSize);
    p = fftwf_plan_dft_1d(fftSize, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);

    sigpath::signalPath.init(sampleRate, 20, fftSize, &soapy.output, (dsp::complex_t*)fft_in, fftHandler);
    sigpath::signalPath.start();

    uiGains = new float[soapy.gainList.size()];

    spdlog::info("Loading modules");
    mod::initAPI(&gui::waterfall);
    mod::loadFromList(ROOT_DIR "/module_list.json");

    bigFont = ImGui::GetIO().Fonts->AddFontFromFileTTF(((std::string)(ROOT_DIR "/res/fonts/Roboto-Medium.ttf")).c_str(), 128.0f);

    // Load last source configuration
    core::configManager.aquire();
    uint64_t frequency = core::configManager.conf["frequency"];
    sourceName = core::configManager.conf["source"];
    auto _sourceIt = std::find(soapy.devNameList.begin(), soapy.devNameList.end(), sourceName);
    if (_sourceIt != soapy.devNameList.end() && core::configManager.conf["sourceSettings"].contains(sourceName)) {
        json sourceSettings = core::configManager.conf["sourceSettings"][sourceName];
        devId = std::distance(soapy.devNameList.begin(), _sourceIt);
        soapy.setDevice(soapy.devList[devId]);
        loadSourceConfig(sourceName);
    }
    else {
        int i = 0;
        bool settingsFound = false;
        for (std::string devName : soapy.devNameList) {
            if (core::configManager.conf["sourceSettings"].contains(devName)) {
                sourceName = devName;
                settingsFound = true;
                devId = i;
                soapy.setDevice(soapy.devList[i]);
                loadSourceConfig(sourceName);
                break;
            }
            i++;
        }
        if (!settingsFound) {
            if (soapy.devNameList.size() > 0) {
                sourceName = soapy.devNameList[0];
            }
            sampleRate = soapy.getSampleRate();
            sigpath::signalPath.setSampleRate(sampleRate);
        }
        // Search for the first source in the list to have a config
        // If no pre-defined source, selected default device
    }

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

    bandPlanEnabled.val = core::configManager.conf["bandPlanEnabled"];
    bandPlanEnabled.markAsChanged();

    std::string bandPlanName = core::configManager.conf["bandPlan"];
    auto _bandplanIt = bandplan::bandplans.find(bandPlanName);
    if (_bandplanIt != bandplan::bandplans.end()) {
        bandplanId.val = std::distance(bandplan::bandplans.begin(), bandplan::bandplans.find(bandPlanName));
        
        if (bandPlanEnabled.val) {
            gui::waterfall.bandplan = &bandplan::bandplans[bandPlanName];
        }
        else {
            gui::waterfall.bandplan = NULL;
        }
    }
    else {
        bandplanId.val = 0;
    }
    bandplanId.markAsChanged();
    

    gui::freqSelect.setFrequency(frequency);
    gui::freqSelect.frequencyChanged = false;
    soapy.setFrequency(frequency);
    gui::waterfall.setCenterFrequency(frequency);
    gui::waterfall.setBandwidth(sampleRate);
    gui::waterfall.setViewBandwidth(sampleRate);
    bw.val = sampleRate;
    gui::waterfall.vfoFreqChanged = false;
    gui::waterfall.centerFreqMoved = false;
    gui::waterfall.selectFirstVFO();

    for (auto [name, stream] : audio::streams) {
        if (core::configManager.conf["audio"].contains(name)) {
            bool running = audio::streams[name]->running;
            audio::stopStream(name);
            loadAudioConfig(name);
            if (running) {
                audio::startStream(name);
            }
        }
    }

    audioStreamName = audio::getNameFromVFO(gui::waterfall.selectedVFO);
    if (audioStreamName != "") {
        volume = &audio::streams[audioStreamName]->volume;
    }

    menuWidth = core::configManager.conf["menuWidth"];
    newWidth = menuWidth;

    showWaterfall = core::configManager.conf["showWaterfall"];
    if (!showWaterfall) {
        gui::waterfall.hideWaterfall();
    }

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
        soapy.setFrequency(freq - newVFOOffset);
        return;
    }

    // VFO too high for current SDR tuning
    if (vfoTop > top) {
        gui::waterfall.setViewOffset((viewBW / 2.0f) - (BW / 2.0f));
        float newVFOOffset = (vfoBW / 2.0f) - (BW / 2.0f) + (viewBW / 10.0f);
        sigpath::vfoManager.setCenterOffset(gui::waterfall.selectedVFO, newVFOOffset);
        gui::waterfall.setCenterFrequency(freq - newVFOOffset);
        soapy.setFrequency(freq - newVFOOffset);
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
        soapy.setFrequency(freq - newVFOOffset);
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
        soapy.setFrequency(freq - newVFOOffset);
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
        mod::broadcastEvent(mod::EVENT_SELECTED_VFO_CHANGED);
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
        soapy.setFrequency(gui::waterfall.getCenterFrequency());
        gui::freqSelect.setFrequency(gui::waterfall.getCenterFrequency() + vfo->generalOffset);
        core::configManager.aquire();
        core::configManager.conf["frequency"] = gui::freqSelect.frequency;
        core::configManager.release(true);
    }

    if (dcbias.changed()) {
        sigpath::signalPath.setDCBiasCorrection(dcbias.val);
    }

    if (bandplanId.changed() && bandPlanEnabled.val) {
        gui::waterfall.bandplan = &bandplan::bandplans[bandplan::bandplanNames[bandplanId.val]];
    }

    if (bandPlanEnabled.changed()) {
        gui::waterfall.bandplan = bandPlanEnabled.val ? &bandplan::bandplans[bandplan::bandplanNames[bandplanId.val]] : NULL;
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

    int modCount = mod::moduleNames.size();
    mod::Module_t mod;
    for (int i = 0; i < modCount; i++) {
        mod = mod::modules[mod::moduleNames[i]];
        mod._NEW_FRAME_(mod.ctx);
    }

    // To Bar
    if (ImGui::ImageButton(icons::MENU, ImVec2(40, 40), ImVec2(0, 0), ImVec2(1, 1), 0)) {
        showMenu = !showMenu;
    }

    ImGui::SameLine();

    if (playing) {
        if (ImGui::ImageButton(icons::STOP, ImVec2(40, 40), ImVec2(0, 0), ImVec2(1, 1), 0)) {
            soapy.stop();
            playing = false;
        }
    }
    else {
        if (ImGui::ImageButton(icons::PLAY, ImVec2(40, 40), ImVec2(0, 0), ImVec2(1, 1), 0) && soapy.devList.size() > 0) {
            soapy.start();
            soapy.setFrequency(gui::waterfall.getCenterFrequency());
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
                saveAudioConfig(audioStreamName);
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

        for (int i = 0; i < modCount; i++) {
            if (ImGui::CollapsingHeader(mod::moduleNames[i].c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                mod = mod::modules[mod::moduleNames[i]];
                mod._DRAW_MENU_(mod.ctx);
                ImGui::Spacing();
            }
        }


        if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen)) {
            int count = 0;
            int maxCount = audio::streams.size();
            for (auto const& [name, stream] : audio::streams) {
                int deviceId;
                float vol = 1.0f;
                deviceId = stream->audio->getDeviceId();

                ImGui::SetCursorPosX((menuColumnWidth / 2.0f) - (ImGui::CalcTextSize(name.c_str()).x / 2.0f));
                ImGui::Text(name.c_str());

                ImGui::PushItemWidth(menuColumnWidth);
                bool running = stream->running;
                if (ImGui::Combo(("##_audio_dev_0_"+ name).c_str(), &stream->deviceId, stream->audio->devTxtList.c_str())) {
                    audio::stopStream(name);
                    audio::setAudioDevice(name, stream->deviceId, stream->audio->devices[deviceId].sampleRates[0]);
                    if (running) {
                        audio::startStream(name);
                    }
                    stream->sampleRateId = 0;

                    // Create config if it doesn't exist
                    core::configManager.aquire();
                    if (!core::configManager.conf["audio"].contains(name)) {
                        saveAudioConfig(name);
                    }
                    core::configManager.conf["audio"][name]["device"] = stream->audio->deviceNames[stream->deviceId];
                    core::configManager.conf["audio"][name]["sampleRate"] = stream->audio->devices[stream->deviceId].sampleRates[0];
                    core::configManager.release(true);
                }
                if (ImGui::Combo(("##_audio_sr_0_" + name).c_str(), &stream->sampleRateId, stream->audio->devices[deviceId].txtSampleRates.c_str())) {
                    audio::stopStream(name);
                    audio::setSampleRate(name, stream->audio->devices[deviceId].sampleRates[stream->sampleRateId]);
                    if (running) {
                        audio::startStream(name);
                    }

                    // Create config if it doesn't exist
                    core::configManager.aquire();
                    if (!core::configManager.conf["audio"].contains(name)) {
                        saveAudioConfig(name);
                    }
                    core::configManager.conf["audio"][name]["sampleRate"] = stream->audio->devices[deviceId].sampleRates[stream->sampleRateId];
                    core::configManager.release(true);
                }
                if (ImGui::SliderFloat(("##_audio_vol_0_" + name).c_str(), &stream->volume, 0.0f, 1.0f, "")) {
                    stream->audio->setVolume(stream->volume);

                    // Create config if it doesn't exist
                    core::configManager.aquire();
                    if (!core::configManager.conf["audio"].contains(name)) {
                        saveAudioConfig(name);
                    }
                    core::configManager.conf["audio"][name]["volume"] = stream->volume;
                    core::configManager.release(true);
                }
                ImGui::PopItemWidth();
                count++;
                if (count < maxCount) {
                    ImGui::Spacing();
                    ImGui::Separator();
                }
                ImGui::Spacing();
            }
            ImGui::Spacing();
        }

        if (ImGui::CollapsingHeader("Band Plan", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::PushItemWidth(menuColumnWidth);
            if (ImGui::Combo("##_4_", &bandplanId.val, bandplan::bandplanNameTxt.c_str())) {
                core::configManager.aquire();
                core::configManager.conf["bandPlan"] = bandplan::bandplanNames[bandplanId.val];
                core::configManager.release(true);
            }
            ImGui::PopItemWidth();
            if (ImGui::Checkbox("Enabled", &bandPlanEnabled.val)) {
                core::configManager.aquire();
                core::configManager.conf["bandPlanEnabled"] = bandPlanEnabled.val;
                core::configManager.release(true);
            }
            bandplan::BandPlan_t plan = bandplan::bandplans[bandplan::bandplanNames[bandplanId.val]];
            ImGui::Text("Country: %s (%s)", plan.countryName.c_str(), plan.countryCode.c_str());
            ImGui::Text("Author: %s", plan.authorName.c_str());
            ImGui::Spacing();
        } 

        if (ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Checkbox("Show waterfall", &showWaterfall)) {
                showWaterfall ? gui::waterfall.showWaterfall() : gui::waterfall.hideWaterfall();
            }
            ImGui::Spacing();
        }

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
    ImGui::VSliderFloat("##_7_", ImVec2(20.0f, 150.0f), &bw.val, sampleRate, 1000.0f, "");

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
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 20.0f));
        ImGui::OpenPopup("Credits");
        ImGui::BeginPopupModal("Credits", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove);

        ImGui::PushFont(bigFont);
        ImGui::Text("SDR++    ");
        ImGui::PopFont();
        ImGui::SameLine();
        ImGui::Image(icons::LOGO, ImVec2(128, 128));
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();

        ImGui::Text("This software is brought to you by\n\n");

        ImGui::Columns(3, "CreditColumns", true);

        // Contributors
        ImGui::Text("Contributors");
        ImGui::BulletText("Ryzerth (Creator)");
        ImGui::BulletText("aosync");
        ImGui::BulletText("Benjamin Kyd");
        ImGui::BulletText("Tobias Mädel");
        ImGui::BulletText("Raov");
        ImGui::BulletText("Howard0su");

        // Libraries
        ImGui::NextColumn();
        ImGui::Text("Libraries");
        ImGui::BulletText("SoapySDR (PothosWare)");
        ImGui::BulletText("Dear ImGui (ocornut)");
        ImGui::BulletText("spdlog (gabime)");
        ImGui::BulletText("json (nlohmann)");
        ImGui::BulletText("portaudio (PA Comm.)");

        // Patrons
        ImGui::NextColumn();
        ImGui::Text("Patrons");
        ImGui::BulletText("SignalsEverywhere");

        ImGui::EndPopup();
        ImGui::PopStyleVar(1);
    }
}

void bindVolumeVariable(float* vol) {
    volume = vol;
}

void unbindVolumeVariable() {
    volume = &dummyVolume;
}