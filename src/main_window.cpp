#include <main_window.h>
#include <imgui_plot.h>
#include <dsp/resampling.h>
#include <dsp/demodulator.h>
#include <dsp/filter.h>
#include <thread>
#include <complex>
#include <dsp/source.h>
#include <dsp/math.h>
#include <waterfall.h>
#include <frequency_select.h>
#include <fftw3.h>
#include <signal_path.h>
#include <io/soapy.h>
#include <icons.h>

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

void windowInit() {
    int sampleRate = 8000000;
    wtf.setBandwidth(sampleRate);
    wtf.setCenterFrequency(90500000);
    wtf.setVFOBandwidth(200000);
    wtf.setVFOOffset(0);

    fSel.init();
    fSel.setFrequency(90500000);
    
    fft_in = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * fftSize);
    fft_out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * fftSize);
    p = fftwf_plan_dft_1d(fftSize, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);

    printf("Starting DSP Thread!\n");

    sigPath.init(sampleRate, 20, fftSize, &soapy.output, (dsp::complex_t*)fft_in, fftHandler);
    sigPath.start();

    uiGains = new float[1];
}

int devId = 0;
int _devId = -1;

int srId = 0;
int _srId = -1;

bool showExample = false;

long freq = 90500000;
long _freq = 90500000;

int demod = 1;

bool state = false;
bool mulstate = true;

float vfoFreq = 92000000.0f;
float lastVfoFreq = 92000000.0f;

float volume = 1.0f;
float lastVolume = 1.0f;

float fftMin = -70.0f;
float fftMax = 0.0f;

float offset = 0.0f;
float lastOffset = -1.0f;
float bw = 8000000.0f;
float lastBW = -1.0f;

int sampleRate = 1000000;

bool playing = false;

bool dcbias = false;
bool _dcbias = false;

void setVFO(float freq) {
    float currentOff =  wtf.getVFOOfset();
    float currentTune = wtf.getCenterFrequency() + currentOff;
    float delta = freq - currentTune;

    float newVFO = currentOff + delta;
    float vfoBW = wtf.getVFOBandwidth();
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
        sigPath.setVFOFrequency(newVFO);
        wtf.setVFOOffset(newVFO);
        return;
    }

    // VFO too low for current SDR tuning
    if (vfoBottom < bottom) {
        wtf.setViewOffset((BW / 2.0f) - (viewBW / 2.0f));
        float newVFOOffset = (BW / 2.0f) - (vfoBW / 2.0f) - (viewBW / 10.0f);
        sigPath.setVFOFrequency(newVFOOffset);
        wtf.setVFOOffset(newVFOOffset);
        wtf.setCenterFrequency(freq - newVFOOffset);
        soapy.setFrequency(freq - newVFOOffset);
        return;
    }

    // VFO too high for current SDR tuning
    if (vfoTop > top) {
        wtf.setViewOffset((viewBW / 2.0f) - (BW / 2.0f));
        float newVFOOffset = (vfoBW / 2.0f) - (BW / 2.0f) + (viewBW / 10.0f);
        sigPath.setVFOFrequency(newVFOOffset);
        wtf.setVFOOffset(newVFOOffset);
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
            wtf.setVFOOffset(newVFO);
            wtf.setViewOffset(newViewOff);
            sigPath.setVFOFrequency(newVFO);
            return;
        }

        wtf.setViewOffset((BW / 2.0f) - (viewBW / 2.0f));
        float newVFOOffset = (BW / 2.0f) - (vfoBW / 2.0f) - (viewBW / 10.0f);
        sigPath.setVFOFrequency(newVFOOffset);
        wtf.setVFOOffset(newVFOOffset);
        wtf.setCenterFrequency(freq - newVFOOffset);
        soapy.setFrequency(freq - newVFOOffset);
    }
    else {
        float newViewOff = vfoBottom + (viewBW / 2.0f) - (viewBW / 10.0f);
        float newViewBottom = newViewOff - (viewBW / 2.0f);
        float newViewTop = newViewOff + (viewBW / 2.0f);

        if (newViewTop < top) {
            wtf.setVFOOffset(newVFO);
            wtf.setViewOffset(newViewOff);
            sigPath.setVFOFrequency(newVFO);
            return;
        }

        wtf.setViewOffset((viewBW / 2.0f) - (BW / 2.0f));
        float newVFOOffset = (vfoBW / 2.0f) - (BW / 2.0f) + (viewBW / 10.0f);
        sigPath.setVFOFrequency(newVFOOffset);
        wtf.setVFOOffset(newVFOOffset);
        wtf.setCenterFrequency(freq - newVFOOffset);
        soapy.setFrequency(freq - newVFOOffset);
    }
}

void drawWindow() {
    if (fSel.frequencyChanged) {
        fSel.frequencyChanged = false;
        setVFO(fSel.frequency);
    }

    if (wtf.centerFreqMoved) {
        wtf.centerFreqMoved = false;
        soapy.setFrequency(wtf.getCenterFrequency());
        fSel.setFrequency(wtf.getCenterFrequency() + wtf.getVFOOfset());
    }
    if (wtf.vfoFreqChanged) {
        wtf.vfoFreqChanged = false;
        sigPath.setVFOFrequency(wtf.getVFOOfset());
        fSel.setFrequency(wtf.getCenterFrequency() + wtf.getVFOOfset());
    }

    if (volume != lastVolume) {
        lastVolume = volume;
        sigPath.setVolume(volume);
    }

    if (devId != _devId && soapy.devList.size() > 0) {
        _devId = devId;
        soapy.setDevice(soapy.devList[devId]);
        srId = 0;
        _srId = -1;
        soapy.setSampleRate(soapy.sampleRates[0]);
        if (soapy.gainList.size() == 0) {
            return;
        }
        delete[] uiGains;
        uiGains = new float[soapy.gainList.size()];
        for (int i = 0; i < soapy.gainList.size(); i++) {
            uiGains[i] = soapy.currentGains[i];
        }
    }

    if (srId != _srId && soapy.devList.size() > 0) {
        _srId = srId;
        sampleRate = soapy.sampleRates[srId];
        printf("Setting sample rate to %f\n", (float)soapy.sampleRates[srId]);
        soapy.setSampleRate(sampleRate);
        wtf.setBandwidth(sampleRate);
        wtf.setViewBandwidth(sampleRate);
        sigPath.setSampleRate(sampleRate);
        bw = sampleRate;
    }

    if (dcbias != _dcbias) {
        _dcbias = dcbias;
        sigPath.setDCBiasCorrection(dcbias);
    }

    ImVec2 vMin = ImGui::GetWindowContentRegionMin();
    ImVec2 vMax = ImGui::GetWindowContentRegionMax();

    int width = vMax.x - vMin.x;
    int height = vMax.y - vMin.y;

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
    ImGui::SliderFloat("##_2_", &volume, 0.0f, 1.0f, "");

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
        ImGui::Combo("##_0_", &devId, soapy.txtDevList.c_str());

        ImGui::PopItemWidth();
        if (!playing) {
            ImGui::Combo("##_1_", &srId, soapy.txtSampleRateList.c_str());
        }
        else {
            ImGui::Text("%.0f Samples/s", soapy.sampleRates[srId]);
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

            // float step = soapy.gainRanges[i].step();
            // printf("%f\n", step);

            // uiGains[i] = roundf(uiGains[i] / soapy.gainRanges[i].step()) * soapy.gainRanges[i].step();

            if (uiGains[i] != soapy.currentGains[i]) {
                soapy.setGain(i, uiGains[i]);
            }
        }
    }

    if (ImGui::CollapsingHeader("Radio")) {
        ImGui::BeginGroup();

        ImGui::Columns(4, "RadioModeColumns", false);
        if (ImGui::RadioButton("NFM", demod == 0) && demod != 0) { 
            sigPath.setDemodulator(SignalPath::DEMOD_NFM); demod = 0; 
            wtf.setVFOBandwidth(12500); 
            wtf.setVFOReference(ImGui::WaterFall::REF_CENTER);
        }
        if (ImGui::RadioButton("WFM", demod == 1) && demod != 1) { 
            sigPath.setDemodulator(SignalPath::DEMOD_FM); 
            demod = 1; 
            wtf.setVFOBandwidth(200000);
            wtf.setVFOReference(ImGui::WaterFall::REF_CENTER);
        }
        ImGui::NextColumn();
        if (ImGui::RadioButton("AM", demod == 2) && demod != 2) { 
            sigPath.setDemodulator(SignalPath::DEMOD_AM); 
            demod = 2; 
            wtf.setVFOBandwidth(12500);
            wtf.setVFOReference(ImGui::WaterFall::REF_CENTER);
        }
        if (ImGui::RadioButton("DSB", demod == 3) && demod != 3) { demod = 3; };
        ImGui::NextColumn();
        if (ImGui::RadioButton("USB", demod == 4) && demod != 4) { 
            sigPath.setDemodulator(SignalPath::DEMOD_USB); 
            demod = 4; 
            wtf.setVFOBandwidth(3000);
            wtf.setVFOReference(ImGui::WaterFall::REF_LOWER);
        }
        if (ImGui::RadioButton("CW", demod == 5) && demod != 5) { demod = 5; };
        ImGui::NextColumn();
        if (ImGui::RadioButton("LSB", demod == 6) && demod != 6) {
            sigPath.setDemodulator(SignalPath::DEMOD_LSB);
            demod = 6;
            wtf.setVFOBandwidth(3000);
            wtf.setVFOReference(ImGui::WaterFall::REF_UPPER);
        }
        if (ImGui::RadioButton("RAW", demod == 7) && demod != 7) { demod = 7; };
        ImGui::Columns(1, "EndRadioModeColumns", false);

        //ImGui::InputInt("Frequency (kHz)", &freq);
        ImGui::Checkbox("DC Bias Removal", &dcbias);

        ImGui::EndGroup();
    }

    ImGui::CollapsingHeader("Audio");

    ImGui::CollapsingHeader("Display");

    ImGui::CollapsingHeader("Recording");

    if(ImGui::CollapsingHeader("Debug")) {
        ImGui::Text("Frame time: %.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);
        ImGui::Text("Framerate: %.1f FPS", ImGui::GetIO().Framerate);
    }

    ImGui::EndChild();

    // Right Column
    ImGui::NextColumn();

    ImGui::BeginChild("Waterfall");
    wtf.draw();    
    ImGui::EndChild();

    ImGui::NextColumn();

    ImGui::Text("Zoom");
    ImGui::VSliderFloat("##_7_", ImVec2(20.0f, 150.0f), &bw, sampleRate, 1000.0f, "");

    ImGui::NewLine();

    ImGui::Text("Max");
    ImGui::VSliderFloat("##_8_", ImVec2(20.0f, 150.0f), &fftMax, 0.0f, -100.0f, "");

    ImGui::NewLine();

    ImGui::Text("Min");
    ImGui::VSliderFloat("##_9_", ImVec2(20.0f, 150.0f), &fftMin, 0.0f, -100.0f, "");

    if (bw != lastBW) {
        lastBW = bw;
        wtf.setViewBandwidth(bw);
        wtf.setViewOffset(wtf.getVFOOfset());
    }

    

    wtf.setFFTMin(fftMin);
    wtf.setFFTMax(fftMax);
    wtf.setWaterfallMin(fftMin);
    wtf.setWaterfallMax(fftMax);
}