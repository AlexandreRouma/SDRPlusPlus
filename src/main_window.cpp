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
#include <fftw3.h>
#include <signal_path.h>
#include <io/soapy.h>

std::thread worker;
std::mutex fft_mtx;
ImGui::WaterFall wtf;
fftwf_complex *fft_in, *fft_out;
fftwf_plan p;
float* tempData;

int fftSize = 8192 * 8;

bool dcbias = true;

io::SoapyWrapper soapy;

//dsp::HackRFSource src;
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
    int sampleRate = 2000000;
    wtf.bandWidth = sampleRate;
    wtf.range = 500000;
    wtf.centerFrequency = 90500000;
    printf("fft taps: %d\n", fftTaps.size());
    
    fft_in = (fftwf_complex*) fftw_malloc(sizeof(fftwf_complex) * fftSize);
    fft_out = (fftwf_complex*) fftw_malloc(sizeof(fftwf_complex) * fftSize);
    p = fftwf_plan_dft_1d(fftSize, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);

    printf("Starting DSP Thread!\n");

    // hackrf_init();
    // hackrf_device_list_t* list = hackrf_device_list();
    
    // int err = hackrf_device_list_open(list, 0, &dev);
    // if (err != 0) {
    //     printf("Error while opening HackRF: %d\n", err);
    //     return;
    // }
    // hackrf_set_freq(dev, 90500000);
    // //hackrf_set_txvga_gain(dev, 10);
    // hackrf_set_amp_enable(dev, 1);
    // hackrf_set_lna_gain(dev, 24);
    // hackrf_set_vga_gain(dev, 20);
    // hackrf_set_baseband_filter_bandwidth(dev, sampleRate);
    // hackrf_set_sample_rate(dev, sampleRate);

    //src.init(dev, 64000);

    sigPath.init(sampleRate, 20, fftSize, &soapy.output, (dsp::complex_t*)fft_in, fftHandler);
    sigPath.start();
}

int devId = 0;
int _devId = -1;

int srId = 0;
int _srId = -1;

bool showExample = false;

int freq = 90500;
int _freq = 90500;

bool state = false;
bool mulstate = true;

float vfoFreq = 92000000.0f;
float lastVfoFreq = 92000000.0f;

float volume = 1.0f;
float lastVolume = 1.0f;

void drawWindow() {
    if (freq != _freq) {
        _freq = freq;
        wtf.centerFrequency = freq * 1000;
        soapy.setFrequency(freq * 1000);
        //hackrf_set_freq(dev, freq * 1000);
    }

    if (vfoFreq != lastVfoFreq) {
        lastVfoFreq = vfoFreq;
        sigPath.setVFOFrequency(vfoFreq - (freq * 1000));
    }

    if (volume != lastVolume) {
        lastVolume = volume;
        sigPath.setVolume(volume);
    }

    if (devId != _devId) {
        _devId = devId;
        soapy.setDevice(soapy.devList[devId]);
    }

    if (srId != _srId) {
        soapy.setSampleRate(soapy.sampleRates[srId]);
    }


    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            ImGui::MenuItem("Show Example Window", "", &showExample);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    if (showExample) {
        ImGui::ShowDemoWindow();
    }


    ImVec2 vMin = ImGui::GetWindowContentRegionMin();
    ImVec2 vMax = ImGui::GetWindowContentRegionMax();

    int width = vMax.x - vMin.x;
    int height = vMax.y - vMin.y;

    ImGui::Columns(2, "WindowColumns", false);
    ImGui::SetColumnWidth(0, 300);

    // Left Column
    ImGui::BeginChild("Left Column");

    if (ImGui::CollapsingHeader("Source")) {
        ImGui::PushItemWidth(ImGui::GetWindowSize().x);
        ImGui::Combo("##_0_", &devId, soapy.txtDevList.c_str());
        ImGui::Combo("##_1_", &srId, soapy.txtSampleRateList.c_str());

        ImGui::SliderFloat("##_2_", &volume, 0.0f, 1.0f, "");
        if (ImGui::Button("Start") && !state) {
            state = true;
            soapy.start();
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop") && state) {
            state = false;
            soapy.stop();
        }
        if (ImGui::Button("Refresh")) {
            soapy.refresh();
        }
    }

    if (ImGui::CollapsingHeader("Radio")) {
        ImGui::BeginGroup();

        ImGui::Columns(4, "RadioModeColumns", false);
        ImGui::RadioButton("NFM", false);
        ImGui::RadioButton("WFM", true);
        ImGui::NextColumn();
        ImGui::RadioButton("AM", false);
        ImGui::RadioButton("DSB", false);
        ImGui::NextColumn();
        ImGui::RadioButton("USB", false);
        ImGui::RadioButton("CW", false);
        ImGui::NextColumn();
        ImGui::RadioButton("LSB", false);
        ImGui::RadioButton("RAW", false);
        ImGui::Columns(1, "EndRadioModeColumns", false);

        ImGui::InputInt("Frequency (kHz)", &freq);
        ImGui::Checkbox("DC Bias Removal", &dcbias);

        ImGui::EndGroup();
    }

    ImGui::CollapsingHeader("Audio");

    ImGui::CollapsingHeader("Display");

    ImGui::CollapsingHeader("Recording");

    if(ImGui::CollapsingHeader("Debug")) {
        ImGui::Text("Frame time: %.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);
        ImGui::Text("Framerate: %.1f FPS", ImGui::GetIO().Framerate);

        if (ImGui::Button("FM demod")) {
            sigPath.setDemodulator(SignalPath::DEMOD_FM);
        }
        if (ImGui::Button("AM demod")) {
            sigPath.setDemodulator(SignalPath::DEMOD_AM);
        }
    }

    ImGui::EndChild();

    // Right Column
    ImGui::NextColumn();

    ImGui::BeginChild("Waterfall");
    wtf.draw(&vfoFreq);    
    ImGui::EndChild();
}