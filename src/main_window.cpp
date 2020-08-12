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

void windowInit() {
    int sampleRate = 8000000;
    wtf.setBandwidth(sampleRate);
    wtf.setCenterFrequency(90500000);

    fSel.init();
    fSel.setFrequency(90500000);
    
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
}

watcher<int> devId(0, true);
watcher<int> srId(0, true);
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
int sampleRate = 1000000;
bool playing = false;
watcher<bool> dcbias(false, false);
watcher<bool> bandPlanEnabled(true, false);

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
    if (wtf.selectedVFO == "" && wtf.vfos.size() > 0) {
        wtf.selectFirstVFO();
    }

    ImGui::WaterfallVFO* vfo = wtf.vfos[wtf.selectedVFO];

    if (vfo->centerOffsetChanged) {
        fSel.setFrequency(wtf.getCenterFrequency() + vfo->generalOffset);
    }

    vfoman::updateFromWaterfall();

    if (wtf.selectedVFOChanged) {
        wtf.selectedVFOChanged = false;
        fSel.setFrequency(vfo->generalOffset + wtf.getCenterFrequency());
        mod::broadcastEvent(mod::EVENT_SELECTED_VFO_CHANGED);
    }

    if (fSel.frequencyChanged) {
        fSel.frequencyChanged = false;
        setVFO(fSel.frequency);
        vfo->centerOffsetChanged = false;
        vfo->lowerOffsetChanged = false;
        vfo->upperOffsetChanged = false;
    }

    if (wtf.centerFreqMoved) {
        wtf.centerFreqMoved = false;
        soapy.setFrequency(wtf.getCenterFrequency());
        fSel.setFrequency(wtf.getCenterFrequency() + vfo->generalOffset);
    }

    if (devId.changed() && soapy.devList.size() > 0) {
        spdlog::info("Changed input device: {0}", devId.val);
        soapy.setDevice(soapy.devList[devId.val]);
        srId.markAsChanged();
        if (soapy.gainList.size() == 0) {
            return;
        }
        delete[] uiGains;
        uiGains = new float[soapy.gainList.size()];
        for (int i = 0; i < soapy.gainList.size(); i++) {
            uiGains[i] = soapy.currentGains[i];
        }
    }

    if (srId.changed() && soapy.devList.size() > 0) {
        spdlog::info("Changed sample rate: {0}", srId.val);
        sampleRate = soapy.sampleRates[srId.val];
        soapy.setSampleRate(sampleRate);
        wtf.setBandwidth(sampleRate);
        wtf.setViewBandwidth(sampleRate);
        sigPath.setSampleRate(sampleRate);
        bw.val = sampleRate;
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
        if (ImGui::ImageButton(icons::STOP_RAW, ImVec2(30, 30))) {
            soapy.stop();
            playing = false;
        }
    }
    else {
        if (ImGui::ImageButton(icons::PLAY_RAW, ImVec2(30, 30)) && soapy.devList.size() > 0) {
            soapy.start();
            soapy.setFrequency(wtf.getCenterFrequency());
            playing = true;
        }
    }

    ImGui::SameLine();

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
    ImGui::SetNextItemWidth(200);
    ImGui::SliderFloat("##_2_", volume, 0.0f, 1.0f, "");

    ImGui::SameLine();

    fSel.draw();

    ImGui::Columns(3, "WindowColumns", false);
    ImVec2 winSize = ImGui::GetWindowSize();
    ImGui::SetColumnWidth(0, 300);
    ImGui::SetColumnWidth(1, winSize.x - 300 - 60);
    ImGui::SetColumnWidth(2, 60);

    // Left Column
    ImGui::BeginChild("Left Column");

    if (ImGui::CollapsingHeader("Source")) {
        ImGui::PushItemWidth(ImGui::GetWindowSize().x);
        ImGui::Combo("##_0_", &devId.val, soapy.txtDevList.c_str());
        ImGui::PopItemWidth();

        if (!playing) {
            ImGui::Combo("##_1_", &srId.val, soapy.txtSampleRateList.c_str());
        }
        else {
            ImGui::Text("%.0f Samples/s", soapy.sampleRates[srId.val]);
        }

        ImGui::SameLine();
        if (ImGui::Button("Refresh")) {
            soapy.refresh();
        }

        for (int i = 0; i < soapy.gainList.size(); i++) {
            ImGui::Text("%s gain", soapy.gainList[i].c_str());
            ImGui::SameLine();
            sprintf(buf, "##_gain_slide_%d_", i);
            ImGui::SliderFloat(buf, &uiGains[i], soapy.gainRanges[i].minimum(), soapy.gainRanges[i].maximum());

            if (uiGains[i] != soapy.currentGains[i]) {
                soapy.setGain(i, uiGains[i]);
            }
        }
    }

    for (int i = 0; i < modCount; i++) {
        if (ImGui::CollapsingHeader(mod::moduleNames[i].c_str())) {
            mod = mod::modules[mod::moduleNames[i]];
            mod._DRAW_MENU_(mod.ctx);
        }
    }

    ImGui::CollapsingHeader("Audio");

    if (ImGui::CollapsingHeader("Band Plan")) {
        ImGui::PushItemWidth(ImGui::GetWindowSize().x);
        ImGui::Combo("##_4_", &bandplanId.val, bandplan::bandplanNameTxt.c_str());
        ImGui::PopItemWidth();
        ImGui::Checkbox("Enabled", &bandPlanEnabled.val);
        bandplan::BandPlan_t plan = bandplan::bandplans[bandplan::bandplanNames[bandplanId.val]];
        ImGui::Text("Country: %s (%s)", plan.countryName, plan.countryCode);
        ImGui::Text("Author: %s", plan.authorName);
    } 

    ImGui::CollapsingHeader("Display");

    ImGui::CollapsingHeader("Recording");

    if(ImGui::CollapsingHeader("Debug")) {
        ImGui::Text("Frame time: %.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);
        ImGui::Text("Framerate: %.1f FPS", ImGui::GetIO().Framerate);
        ImGui::Text("Center Frequency: %.1f FPS", wtf.getCenterFrequency());
    }

    ImGui::EndChild();

    // Right Column
    ImGui::NextColumn();

    ImGui::BeginChild("Waterfall");
    wtf.draw();    
    ImGui::EndChild();

    ImGui::NextColumn();

    ImGui::Text("Zoom");
    ImGui::VSliderFloat("##_7_", ImVec2(20.0f, 150.0f), &bw.val, sampleRate, 1000.0f, "");

    ImGui::NewLine();

    ImGui::Text("Max");
    ImGui::VSliderFloat("##_8_", ImVec2(20.0f, 150.0f), &fftMax, 0.0f, -100.0f, "");

    ImGui::NewLine();

    ImGui::Text("Min");
    ImGui::VSliderFloat("##_9_", ImVec2(20.0f, 150.0f), &fftMin, 0.0f, -100.0f, "");

    if (bw.changed()) {
        wtf.setViewBandwidth(bw.val);
        wtf.setViewOffset(vfo->centerOffset);
    }

    wtf.setFFTMin(fftMin);
    wtf.setFFTMax(fftMax);
    wtf.setWaterfallMin(fftMin);
    wtf.setWaterfallMax(fftMax);
}

void bindVolumeVariable(float* vol) {
    volume = vol;
}

void unbindVolumeVariable() {
    volume = &dummyVolume;
}