#include <main_window.h>

std::thread worker;
std::mutex fft_mtx;
ImGui::WaterFall wtf;
FrequencySelect fSel;
fftwf_complex *fft_in, *fft_out;
fftwf_plan p;
float* tempData;
float* uiGains;
char buf[1024];
ImFont* bigFont;

int fftSize = 8192 * 8;

io::SoapyWrapper soapy;

SignalPath sigPath;
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

    wtf.pushFFT(_data, fftSize);
    _data.clear();
}

dsp::NullSink sink;
int devId = 0;
int srId = 0;
watcher<int> bandplanId(0, true);
watcher<long> freq(90500000L);
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

void saveCurrentSource() {
    int i = 0;
    for (std::string gainName : soapy.gainList) {
        config::config["sourceSettings"][sourceName]["gains"][gainName] = uiGains[i];
        i++;
    }
    config::config["sourceSettings"][sourceName]["sampleRate"] = soapy.sampleRates[srId];
}

void loadSourceConfig(std::string name) {
    json sourceSettings = config::config["sourceSettings"][name];

    // Set sample rate

    spdlog::warn("Type {0}", sourceSettings.contains("sampleRate"));

    sampleRate = sourceSettings["sampleRate"];

    sigPath.setSampleRate(sampleRate);
    soapy.setSampleRate(sampleRate);
    auto _srIt = std::find(soapy.sampleRates.begin(), soapy.sampleRates.end(), sampleRate);
    srId = std::distance(soapy.sampleRates.begin(), _srIt);
    spdlog::warn("sr {0}", srId);

    // Set gains
    delete uiGains;
    uiGains = new float[soapy.gainList.size()];
    int i = 0;
    for (std::string gainName : soapy.gainList) {
        uiGains[i] = sourceSettings["gains"][gainName];
        soapy.setGain(i, uiGains[i]);
        i++;
    }

    // Update GUI
    wtf.setBandwidth(sampleRate);
    wtf.setViewBandwidth(sampleRate);
    bw.val = sampleRate;
}

void windowInit() {
    fSel.init();
    
    fft_in = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * fftSize);
    fft_out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * fftSize);
    p = fftwf_plan_dft_1d(fftSize, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);

    sigPath.init(sampleRate, 20, fftSize, &soapy.output, (dsp::complex_t*)fft_in, fftHandler);
    sigPath.start();

    vfoman::init(&wtf, &sigPath);

    uiGains = new float[1];

    spdlog::info("Loading modules");
    mod::initAPI(&wtf);
    mod::loadFromList("module_list.json");

    bigFont = ImGui::GetIO().Fonts->AddFontFromFileTTF("res/fonts/Roboto-Medium.ttf", 128.0f);

    // Load last source configuration
    uint64_t frequency = config::config["frequency"];
    sourceName = config::config["source"];
    auto _sourceIt = std::find(soapy.devNameList.begin(), soapy.devNameList.end(), sourceName);
    if (_sourceIt != soapy.devNameList.end() && config::config["sourceSettings"].contains(sourceName)) {
        json sourceSettings = config::config["sourceSettings"][sourceName];
        devId = std::distance(soapy.devNameList.begin(), _sourceIt);
        soapy.setDevice(soapy.devList[devId]);
        loadSourceConfig(sourceName);
    }
    else {
        int i = 0;
        bool settingsFound = false;
        for (std::string devName : soapy.devNameList) {
            if (config::config["sourceSettings"].contains(devName)) {
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
            sampleRate = soapy.getSampleRate();
        }
        // Search for the first source in the list to have a config
        // If no pre-defined source, selected default device
    }

    // Load last band plan configuration

    // TODO: Save/load config for audio streams + window size/fullscreen


    // Update UI settings
    fftMin = config::config["min"];
    fftMax = config::config["max"];
    wtf.setFFTMin(fftMin);
    wtf.setWaterfallMin(fftMin);
    wtf.setFFTMax(fftMax);
    wtf.setWaterfallMax(fftMax);

    bandPlanEnabled.val = config::config["bandPlanEnabled"];
    bandPlanEnabled.markAsChanged();

    std::string bandPlanName = config::config["bandPlan"];
    auto _bandplanIt = bandplan::bandplans.find(bandPlanName);
    if (_bandplanIt != bandplan::bandplans.end()) {
        bandplanId.val = std::distance(bandplan::bandplans.begin(), bandplan::bandplans.find(bandPlanName));
        spdlog::warn("{0} => {1}", bandplanId.val, bandPlanName);
        
        if (bandPlanEnabled.val) {
            wtf.bandplan = &bandplan::bandplans[bandPlanName];
        }
        else {
            wtf.bandplan = NULL;
        }
    }
    else {
        bandplanId.val = 0;
    }
    bandplanId.markAsChanged();
    

    fSel.setFrequency(frequency);
    fSel.frequencyChanged = false;
    soapy.setFrequency(frequency);
    wtf.setCenterFrequency(frequency);
    wtf.setBandwidth(sampleRate);
    wtf.setViewBandwidth(sampleRate);
    bw.val = sampleRate;
    wtf.vfoFreqChanged = false;
    wtf.centerFreqMoved = false;
    wtf.selectFirstVFO();


    audioStreamName = audio::getNameFromVFO(wtf.selectedVFO);
    if (audioStreamName != "") {
        volume = &audio::streams[audioStreamName]->volume;
    }
}

void setVFO(float freq) {
    ImGui::WaterfallVFO* vfo = wtf.vfos[wtf.selectedVFO];

    float currentOff =  vfo->centerOffset;
    float currentTune = wtf.getCenterFrequency() + vfo->generalOffset;
    float delta = freq - currentTune;

    float newVFO = currentOff + delta;
    float vfoBW = vfo->bandwidth;
    float vfoBottom = newVFO - (vfoBW / 2.0f);
    float vfoTop = newVFO + (vfoBW / 2.0f);

    float view = wtf.getViewOffset();
    float viewBW = wtf.getViewBandwidth();
    float viewBottom = view - (viewBW / 2.0f);
    float viewTop = view + (viewBW / 2.0f);

    float wholeFreq = wtf.getCenterFrequency();
    float BW = wtf.getBandwidth();
    float bottom = -(BW / 2.0f);
    float top = (BW / 2.0f);

    // VFO still fints in the view
    if (vfoBottom > viewBottom && vfoTop < viewTop) {
        vfoman::setCenterOffset(wtf.selectedVFO, newVFO);
        return;
    }

    // VFO too low for current SDR tuning
    if (vfoBottom < bottom) {
        wtf.setViewOffset((BW / 2.0f) - (viewBW / 2.0f));
        float newVFOOffset = (BW / 2.0f) - (vfoBW / 2.0f) - (viewBW / 10.0f);
        vfoman::setCenterOffset(wtf.selectedVFO, newVFOOffset);
        wtf.setCenterFrequency(freq - newVFOOffset);
        soapy.setFrequency(freq - newVFOOffset);
        return;
    }

    // VFO too high for current SDR tuning
    if (vfoTop > top) {
        wtf.setViewOffset((viewBW / 2.0f) - (BW / 2.0f));
        float newVFOOffset = (vfoBW / 2.0f) - (BW / 2.0f) + (viewBW / 10.0f);
        vfoman::setCenterOffset(wtf.selectedVFO, newVFOOffset);
        wtf.setCenterFrequency(freq - newVFOOffset);
        soapy.setFrequency(freq - newVFOOffset);
        return;
    }

    // VFO is still without the SDR's bandwidth
    if (delta < 0) {
        float newViewOff = vfoTop - (viewBW / 2.0f) + (viewBW / 10.0f);
        float newViewBottom = newViewOff - (viewBW / 2.0f);
        float newViewTop = newViewOff + (viewBW / 2.0f);

        if (newViewBottom > bottom) {
            wtf.setViewOffset(newViewOff);
            vfoman::setCenterOffset(wtf.selectedVFO, newVFO);
            return;
        }

        wtf.setViewOffset((BW / 2.0f) - (viewBW / 2.0f));
        float newVFOOffset = (BW / 2.0f) - (vfoBW / 2.0f) - (viewBW / 10.0f);
        vfoman::setCenterOffset(wtf.selectedVFO, newVFOOffset);
        wtf.setCenterFrequency(freq - newVFOOffset);
        soapy.setFrequency(freq - newVFOOffset);
    }
    else {
        float newViewOff = vfoBottom + (viewBW / 2.0f) - (viewBW / 10.0f);
        float newViewBottom = newViewOff - (viewBW / 2.0f);
        float newViewTop = newViewOff + (viewBW / 2.0f);

        if (newViewTop < top) {
            wtf.setViewOffset(newViewOff);
            vfoman::setCenterOffset(wtf.selectedVFO, newVFO);
            return;
        }

        wtf.setViewOffset((viewBW / 2.0f) - (BW / 2.0f));
        float newVFOOffset = (vfoBW / 2.0f) - (BW / 2.0f) + (viewBW / 10.0f);
        vfoman::setCenterOffset(wtf.selectedVFO, newVFOOffset);
        wtf.setCenterFrequency(freq - newVFOOffset);
        soapy.setFrequency(freq - newVFOOffset);
    }
}

void drawWindow() {
    ImGui::Begin("Main", NULL, WINDOW_FLAGS);

    ImGui::WaterfallVFO* vfo = wtf.vfos[wtf.selectedVFO];

    if (vfo->centerOffsetChanged) {
        fSel.setFrequency(wtf.getCenterFrequency() + vfo->generalOffset);
        fSel.frequencyChanged = false;
        config::config["frequency"] = fSel.frequency;
        config::configModified = true;
    }

    vfoman::updateFromWaterfall();

    if (wtf.selectedVFOChanged) {
        wtf.selectedVFOChanged = false;
        fSel.setFrequency(vfo->generalOffset + wtf.getCenterFrequency());
        fSel.frequencyChanged = false;
        mod::broadcastEvent(mod::EVENT_SELECTED_VFO_CHANGED);
        audioStreamName = audio::getNameFromVFO(wtf.selectedVFO);
        if (audioStreamName != "") {
            volume = &audio::streams[audioStreamName]->volume;
        }
        config::config["frequency"] = fSel.frequency;
        config::configModified = true;
    }

    if (fSel.frequencyChanged) {
        fSel.frequencyChanged = false;
        setVFO(fSel.frequency);
        vfo->centerOffsetChanged = false;
        vfo->lowerOffsetChanged = false;
        vfo->upperOffsetChanged = false;
        config::config["frequency"] = fSel.frequency;
        config::configModified = true;
    }

    if (wtf.centerFreqMoved) {
        wtf.centerFreqMoved = false;
        soapy.setFrequency(wtf.getCenterFrequency());
        fSel.setFrequency(wtf.getCenterFrequency() + vfo->generalOffset);
        config::config["frequency"] = fSel.frequency;
        config::configModified = true;
    }

    if (dcbias.changed()) {
        sigPath.setDCBiasCorrection(dcbias.val);
    }

    if (bandplanId.changed() && bandPlanEnabled.val) {
        wtf.bandplan = &bandplan::bandplans[bandplan::bandplanNames[bandplanId.val]];
    }

    if (bandPlanEnabled.changed()) {
        wtf.bandplan = bandPlanEnabled.val ? &bandplan::bandplans[bandplan::bandplanNames[bandplanId.val]] : NULL;
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
    if (playing) {
        if (ImGui::ImageButton(icons::STOP, ImVec2(40, 40), ImVec2(0, 0), ImVec2(1, 1), 0)) {
            soapy.stop();
            playing = false;
        }
    }
    else {
        if (ImGui::ImageButton(icons::PLAY, ImVec2(40, 40), ImVec2(0, 0), ImVec2(1, 1), 0) && soapy.devList.size() > 0) {
            soapy.start();
            soapy.setFrequency(wtf.getCenterFrequency());
            playing = true;
        }
    }

    ImGui::SameLine();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
    ImGui::SetNextItemWidth(200);
    if (ImGui::SliderFloat("##_2_", volume, 0.0f, 1.0f, "")) {
        if (audioStreamName != "") {
            audio::streams[audioStreamName]->audio->setVolume(*volume);
        }
    }

    ImGui::SameLine();

    fSel.draw();

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

    ImGui::Columns(3, "WindowColumns", false);
    ImVec2 winSize = ImGui::GetWindowSize();
    ImGui::SetColumnWidth(0, 300);
    ImGui::SetColumnWidth(1, winSize.x - 300 - 60);
    ImGui::SetColumnWidth(2, 60);

    // Left Column
    ImGui::BeginChild("Left Column");
    float menuColumnWidth = ImGui::GetContentRegionAvailWidth();

    if (ImGui::CollapsingHeader("Source", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (playing) { style::beginDisabled(); };

        ImGui::PushItemWidth(menuColumnWidth);
        if (ImGui::Combo("##_0_", &devId, soapy.txtDevList.c_str())) {
            spdlog::info("Changed input device: {0}", devId);
            sourceName = soapy.devNameList[devId];
            soapy.setDevice(soapy.devList[devId]);
            if (soapy.gainList.size() == 0) {
                return;
            }
            delete[] uiGains;
            uiGains = new float[soapy.gainList.size()];
            for (int i = 0; i < soapy.gainList.size(); i++) {
                uiGains[i] = soapy.currentGains[i];
            }

            if (config::config["sourceSettings"].contains(sourceName)) {
                loadSourceConfig(sourceName);
            }
            else {
                srId = 0;
                sampleRate = soapy.getSampleRate();
                bw.val = sampleRate;
                wtf.setBandwidth(sampleRate);
                wtf.setViewBandwidth(sampleRate);
                sigPath.setSampleRate(sampleRate);
                for (int i = 0; i < soapy.gainList.size(); i++) {
                    uiGains[i] = soapy.gainRanges[i].minimum();
                }
            }
            setVFO(fSel.frequency);
            config::config["source"] = sourceName;
            config::configModified = true;
        }
        ImGui::PopItemWidth();

        if (ImGui::Combo("##_1_", &srId, soapy.txtSampleRateList.c_str())) {
            spdlog::info("Changed sample rate: {0}", srId);
            sampleRate = soapy.sampleRates[srId];
            soapy.setSampleRate(sampleRate);
            wtf.setBandwidth(sampleRate);
            wtf.setViewBandwidth(sampleRate);
            sigPath.setSampleRate(sampleRate);
            bw.val = sampleRate;

            if (!config::config["sourceSettings"].contains(sourceName)) {
                saveCurrentSource();
            }
            config::config["sourceSettings"][sourceName]["sampleRate"] = sampleRate;
            config::configModified = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Refresh", ImVec2(menuColumnWidth - ImGui::GetCursorPosX(), 0.0f))) {
            soapy.refresh();
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
                config::config["sourceSettings"][sourceName]["gains"][soapy.gainList[i]] = uiGains[i];
                config::configModified = true;
            }
            ImGui::PopItemWidth();

            if (uiGains[i] != soapy.currentGains[i]) {
                
            }
        }

        ImGui::Spacing();
    }

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
            if (ImGui::Combo(("##_audio_dev_0_"+ name).c_str(), &stream->deviceId, stream->audio->devTxtList.c_str())) {
                audio::stopStream(name);
                audio::setAudioDevice(name, stream->deviceId, stream->audio->devices[deviceId].sampleRates[0]);
                audio::startStream(name);
                stream->sampleRateId = 0;
            }
            if (ImGui::Combo(("##_audio_sr_0_" + name).c_str(), &stream->sampleRateId, stream->audio->devices[deviceId].txtSampleRates.c_str())) {
                audio::stopStream(name);
                audio::setSampleRate(name, stream->audio->devices[deviceId].sampleRates[stream->sampleRateId]);
                audio::startStream(name);
            }
            if (ImGui::SliderFloat(("##_audio_vol_0_" + name).c_str(), &stream->volume, 0.0f, 1.0f, "")) {
                stream->audio->setVolume(stream->volume);
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
            config::config["bandPlan"] = bandplan::bandplanNames[bandplanId.val];
            config::configModified = true;
        }
        ImGui::PopItemWidth();
        if (ImGui::Checkbox("Enabled", &bandPlanEnabled.val)) {
            config::config["bandPlanEnabled"] = bandPlanEnabled.val;
            config::configModified = true;
        }
        bandplan::BandPlan_t plan = bandplan::bandplans[bandplan::bandplanNames[bandplanId.val]];
        ImGui::Text("Country: %s (%s)", plan.countryName, plan.countryCode);
        ImGui::Text("Author: %s", plan.authorName);
        ImGui::Spacing();
    } 

    if (ImGui::CollapsingHeader("Display")) {
        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader("Recording")) {
        ImGui::Spacing();
    }

    if(ImGui::CollapsingHeader("Debug")) {
        ImGui::Text("Frame time: %.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);
        ImGui::Text("Framerate: %.1f FPS", ImGui::GetIO().Framerate);
        ImGui::Text("Center Frequency: %.0f Hz", wtf.getCenterFrequency());

        ImGui::Spacing();
    }

    ImGui::EndChild();

    // Right Column
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::NextColumn();
    ImGui::BeginChild("Waterfall");

    wtf.draw();    

    ImGui::EndChild();

    ImGui::PopStyleVar();
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
        config::config["max"] = fftMax;
        config::configModified = true;
    }

    ImGui::NewLine();

    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0f) - (ImGui::CalcTextSize("Min").x / 2.0f));
    ImGui::Text("Min");
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x / 2.0f) - 10);
    if (ImGui::VSliderFloat("##_9_", ImVec2(20.0f, 150.0f), &fftMin, 0.0f, -100.0f, "")) {
        config::config["min"] = fftMin;
        config::configModified = true;
    }

    ImGui::EndChild();

    if (bw.changed()) {
        wtf.setViewBandwidth(bw.val);
        wtf.setViewOffset(vfo->centerOffset);
    }

    wtf.setFFTMin(fftMin);
    wtf.setFFTMax(fftMax);
    wtf.setWaterfallMin(fftMin);
    wtf.setWaterfallMax(fftMax);

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