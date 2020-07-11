#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <vector>
#include <mutex>
#include <GL/glew.h>

#define WATERFALL_RESOLUTION    1000000

namespace ImGui {

    class WaterFall {
    public:
        WaterFall();

        void draw();
        void pushFFT(std::vector<float> data, int n);

        void updatePallette(float colors[][3], int colorCount);

        void setCenterFrequency(float freq);
        float getCenterFrequency();

        void setBandwidth(float bandWidth);
        float getBandwidth();

        void setVFOOffset(float offset);
        float getVFOOfset();

        void setVFOBandwidth(float bandwidth);
        float getVFOBandwidth();

        void setViewBandwidth(float bandWidth);
        void setViewOffset(float offset);

        void setFFTMin(float min);
        float getFFTMin();

        void setFFTMax(float max);
        float getFFTMax();

        void setWaterfallMin(float min);
        float getWaterfallMin();

        void setWaterfallMax(float max);
        float getWaterfallMax();

        void setZoom(float zoomLevel);
        void setOffset(float zoomOffset);

        void autoRange();


    private:
        void drawWaterfall();
        void drawFFT();
        void onPositionChange();
        void onResize();
        void updateWaterfallFb();
        void updateWaterfallTexture();

        bool waterfallUpdate = false;

        uint32_t waterfallPallet[WATERFALL_RESOLUTION];
        
        ImVec2 widgetPos;
        ImVec2 widgetEndPos;
        ImVec2 widgetSize;

        ImVec2 lastWidgetPos;
        ImVec2 lastWidgetSize;

        ImGuiWindow* window;

        GLuint textureId;

        std::mutex buf_mtx;

        int dataWidth;          // Width of the FFT and waterfall
        int fftHeight;          // Height of the fft graph
        int waterfallHeight;    // Height of the waterfall

        float viewBandwidth;
        float viewOffset;

        float lowerFreq;
        float upperFreq;
        float range;

        // Absolute values
        float centerFreq;
        float wholeBandwidth;

        // VFO
        float vfoOffset;
        float vfoBandwidth;

        // Ranges
        float fftMin;
        float fftMax;
        float waterfallMin;
        float waterfallMax;

        std::vector<std::vector<float>> rawFFTs;
        float* latestFFT;

        uint32_t* waterfallFb;

    };
};