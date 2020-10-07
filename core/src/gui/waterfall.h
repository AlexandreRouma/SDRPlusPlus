#pragma once
#include <vector>
#include <mutex>
#include <gui/bandplan.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <GL/glew.h>

#define WATERFALL_RESOLUTION    1000000

namespace ImGui {

    class WaterfallVFO {
    public:
        void setOffset(float offset);
        void setCenterOffset(float offset);
        void setBandwidth(float bw);
        void setReference(int ref);
        void updateDrawingVars(float viewBandwidth, float dataWidth, float viewOffset, ImVec2 widgetPos, int fftHeight);
        void draw(ImGuiWindow* window, bool selected);

        enum {
            REF_LOWER,
            REF_CENTER,
            REF_UPPER,
            _REF_COUNT
        };

        float generalOffset;
        float centerOffset;
        float lowerOffset;
        float upperOffset;
        float bandwidth;
        float snapInterval;
        int reference = REF_CENTER;

        ImVec2 rectMin;
        ImVec2 rectMax;
        ImVec2 lineMin;
        ImVec2 lineMax;

        bool centerOffsetChanged = false;
        bool lowerOffsetChanged = false;
        bool upperOffsetChanged = false;
        bool redrawRequired = true;
        bool lineVisible = true;
    };

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

        void setViewBandwidth(float bandWidth);
        float getViewBandwidth();

        void setViewOffset(float offset);
        float getViewOffset();

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

        void selectFirstVFO();

        void showWaterfall();
        void hideWaterfall();

        void showBandplan();
        void hideBandplan();

        void setFFTHeight(int height);
        int getFFTHeight();

        bool centerFreqMoved = false;
        bool vfoFreqChanged = false;
        bool bandplanEnabled = false;
        bandplan::BandPlan_t* bandplan = NULL;

        std::map<std::string, WaterfallVFO*> vfos;
        std::string selectedVFO;
        bool selectedVFOChanged = false;

        enum {
            REF_LOWER,
            REF_CENTER,
            REF_UPPER,
            _REF_COUNT
        };


    private:
        void drawWaterfall();
        void drawFFT();
        void drawVFOs();
        void drawBandPlan();
        void processInputs();
        void onPositionChange();
        void onResize();
        void updateWaterfallFb();
        void updateWaterfallTexture();
        void updateAllVFOs();

        bool waterfallUpdate = false;

        uint32_t waterfallPallet[WATERFALL_RESOLUTION];
        
        ImVec2 widgetPos;
        ImVec2 widgetEndPos;
        ImVec2 widgetSize;

        ImVec2 lastWidgetPos;
        ImVec2 lastWidgetSize;

        ImVec2 fftAreaMin;
        ImVec2 fftAreaMax;
        ImVec2 freqAreaMin;
        ImVec2 freqAreaMax;
        ImVec2 waterfallAreaMin;
        ImVec2 waterfallAreaMax;

        ImGuiWindow* window;

        GLuint textureId;

        std::mutex buf_mtx;

        float vRange;

        int maxVSteps;
        int maxHSteps;

        int dataWidth;          // Width of the FFT and waterfall
        int fftHeight;          // Height of the fft graph
        int waterfallHeight;    // Height of the waterfall

        float viewBandwidth;
        float viewOffset;

        float lowerFreq;
        float upperFreq;
        float range;

        float lastDrag;

        int vfoRef = REF_CENTER;

        // Absolute values
        float centerFreq;
        float wholeBandwidth;

        // Ranges
        float fftMin;
        float fftMax;
        float waterfallMin;
        float waterfallMax;

        std::vector<std::vector<float>> rawFFTs;
        float* latestFFT;

        uint32_t* waterfallFb;

        bool draggingFW = false;
        int FFTAreaHeight;
        int newFFTAreaHeight;

        bool waterfallVisible = true;
        bool bandplanVisible = false;
    };
};