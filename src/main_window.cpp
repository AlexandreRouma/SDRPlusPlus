#include <main_window.h>
#include <imgui_plot.h>
#include <hackrf.h>
#include <cdsp/hackrf.h>
#include <cdsp/resampling.h>
#include <cdsp/demodulation.h>
#include <cdsp/filter.h>
#include <thread>
#include <complex>
#include <cdsp/generator.h>
#include <cdsp/math.h>
#include <waterfall.h>
#include <fftw3.h>
#include <signal_path.h>

std::thread worker;
std::mutex fft_mtx;
ImGui::WaterFall wtf;
hackrf_device* dev;
fftwf_complex *fft_in, *fft_out;
fftwf_plan p;

bool dcbias = true;

void windowInit() {
    int fftSize = 8192;
    wtf.bandWidth = 8000000;
    wtf.range = 500000;
    
    fft_in = (fftwf_complex*) fftw_malloc(sizeof(fftw_complex) * fftSize);
    fft_out = (fftwf_complex*) fftw_malloc(sizeof(fftw_complex) * fftSize);
    p = fftwf_plan_dft_1d(fftSize, fft_in, fft_out, FFTW_FORWARD, FFTW_ESTIMATE);

    worker = std::thread([=]() {
        std::vector<float> data;
        printf("Starting DSP Thread!\n");

        hackrf_init();
        hackrf_device_list_t* list = hackrf_device_list();
        
        int err = hackrf_device_list_open(list, 0, &dev);
        if (err != 0) {
            printf("Error while opening HackRF: %d\n", err);
            return -1;
        }
        hackrf_set_freq(dev, 98000000);
        //hackrf_set_txvga_gain(dev, 10);
        hackrf_set_amp_enable(dev, 0);
        hackrf_set_lna_gain(dev, 24);
        hackrf_set_vga_gain(dev, 20);
        hackrf_set_sample_rate(dev, 8000000);

        cdsp::HackRFSource src(dev, 64000);
        cdsp::DCBiasRemover bias(&src.output, 64000);
        cdsp::ComplexSineSource sinsrc(4000000.0f, 8000000, 64000);
        cdsp::BlockDecimator dec(&bias.output, 566603 - fftSize, fftSize);
        cdsp::Multiplier mul(&dec.output, &sinsrc.output, fftSize);
        
        src.start();
        bias.start();
        sinsrc.start();
        dec.start();
        mul.start();

        while (true) {
            mul.output.read((cdsp::complex_t*)fft_in, fftSize);

            fftwf_execute(p);

            for (int i = 0; i < fftSize ; i++) {
                data.push_back(log10(std::abs(std::complex<float>(fft_out[i][0], fft_out[i][1])) / (float)fftSize) * 10.0f);
            }

            for (int i = 5; i < fftSize; i++) {
                data[i] = (data[i - 3] + data[i - 2] + data[i - 1] + data[i]) / 4.0f;
            }

            bias.bypass = !dcbias;

            wtf.pushFFT(data, fftSize);
            data.clear();
        }
    });
}

int Current = 0;
bool showExample = false;

int freq = 98000;
int _freq = 98000;

void drawWindow() {
    if (freq != _freq) {
        _freq = freq;
        wtf.centerFrequency = freq * 1000;
        hackrf_set_freq(dev, freq * 1000);
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
        
        ImGui::Combo("Source", &Current, "HackRF One\0RTL-SDR");
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
    }

    ImGui::EndChild();

    // Right Column
    ImGui::NextColumn();

    ImGui::BeginChild("Waterfall");
    wtf.draw();    
    ImGui::EndChild();
}