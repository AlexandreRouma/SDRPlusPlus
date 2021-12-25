#pragma once
#include <vector>
#include <mutex>
#include <gui/widgets/bandplan.h>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <GL/glew.h>
#include <utils/event.h>

#define WATERFALL_RESOLUTION 1000000

namespace ImGui {
    class WaterfallVFO {
    public:
        void setOffset(double offset);
        void setCenterOffset(double offset);
        void setBandwidth(double bw);
        void setReference(int ref);
        void setSnapInterval(double interval);
        void setNotchOffset(double offset);
        void setNotchVisible(bool visible);
        void updateDrawingVars(double viewBandwidth, float dataWidth, double viewOffset, ImVec2 widgetPos, int fftHeight); // NOTE: Datawidth double???
        void draw(ImGuiWindow* window, bool selected);

        enum {
            REF_LOWER,
            REF_CENTER,
            REF_UPPER,
            _REF_COUNT
        };

        double generalOffset;
        double centerOffset;
        double lowerOffset;
        double upperOffset;
        double bandwidth;
        double snapInterval = 5000;
        int reference = REF_CENTER;

        double notchOffset = 0;
        bool notchVisible = false;

        bool leftClamped;
        bool rightClamped;

        ImVec2 rectMin;
        ImVec2 rectMax;
        ImVec2 lineMin;
        ImVec2 lineMax;
        ImVec2 wfRectMin;
        ImVec2 wfRectMax;
        ImVec2 wfLineMin;
        ImVec2 wfLineMax;
        ImVec2 lbwSelMin;
        ImVec2 lbwSelMax;
        ImVec2 rbwSelMin;
        ImVec2 rbwSelMax;
        ImVec2 wfLbwSelMin;
        ImVec2 wfLbwSelMax;
        ImVec2 wfRbwSelMin;
        ImVec2 wfRbwSelMax;
        ImVec2 notchMin;
        ImVec2 notchMax;

        bool centerOffsetChanged = false;
        bool lowerOffsetChanged = false;
        bool upperOffsetChanged = false;
        bool redrawRequired = true;
        bool lineVisible = true;
        bool bandwidthChanged = false;

        double minBandwidth;
        double maxBandwidth;
        bool bandwidthLocked;

        ImU32 color = IM_COL32(255, 255, 255, 50);

        Event<double> onUserChangedBandwidth;
        Event<double> onUserChangedNotch;
    };

    class WaterFall {
    public:
        WaterFall();

        void init();

        void draw();
        float* getFFTBuffer();
        void pushFFT();

        inline void doZoom(int offset, int width, int outWidth, float* data, float* out, bool fast) {
            // NOTE: REMOVE THAT SHIT, IT'S JUST A HACKY FIX
            if (offset < 0) {
                offset = 0;
            }
            if (width > 524288) {
                width = 524288;
            }

            float factor = (float)width / (float)outWidth;

            if (fast) {
                for (int i = 0; i < outWidth; i++) {
                    out[i] = data[(int)(offset + ((float)i * factor))];
                }
            }
            else {
                float sFactor = ceilf(factor);
                float uFactor;
                float id = offset;
                float val, maxVal;
                int sId;
                uint32_t maxId;
                for (int i = 0; i < outWidth; i++) {
                    maxVal = -INFINITY;
                    sId = (int)id;
                    uFactor = (sId + sFactor > rawFFTSize) ? sFactor - ((sId + sFactor) - rawFFTSize) : sFactor;
                    for (int j = 0; j < uFactor; j++) {
                        if (data[sId + j] > maxVal) { maxVal = data[sId + j]; }
                    }
                    out[i] = maxVal;
                    id += factor;
                }
            }
        }

        void updatePallette(float colors[][3], int colorCount);
        void updatePalletteFromArray(float* colors, int colorCount);

        void setCenterFrequency(double freq);
        double getCenterFrequency();

        void setBandwidth(double bandWidth);
        double getBandwidth();

        void setViewBandwidth(double bandWidth);
        double getViewBandwidth();

        void setViewOffset(double offset);
        double getViewOffset();

        void setFFTMin(float min);
        float getFFTMin();

        void setFFTMax(float max);
        float getFFTMax();

        void setWaterfallMin(float min);
        float getWaterfallMin();

        void setWaterfallMax(float max);
        float getWaterfallMax();

        void setZoom(double zoomLevel);
        void setOffset(double zoomOffset);

        void autoRange();

        void selectFirstVFO();

        void showWaterfall();
        void hideWaterfall();

        void showBandplan();
        void hideBandplan();

        void setFFTHeight(int height);
        int getFFTHeight();

        void setRawFFTSize(int size);

        void setFastFFT(bool fastFFT);

        void setFullWaterfallUpdate(bool fullUpdate);

        void setBandPlanPos(int pos);

        bool centerFreqMoved = false;
        bool vfoFreqChanged = false;
        bool bandplanEnabled = false;
        bandplan::BandPlan_t* bandplan = NULL;

        bool mouseInFFTResize = false;
        bool mouseInFreq = false;
        bool mouseInFFT = false;
        bool mouseInWaterfall = false;

        float selectedVFOSNR = NAN;

        bool centerFrequencyLocked = false;

        std::map<std::string, WaterfallVFO*> vfos;
        std::string selectedVFO = "";
        bool selectedVFOChanged = false;

        struct FFTRedrawArgs {
            ImVec2 min;
            ImVec2 max;
            double lowFreq;
            double highFreq;
            double freqToPixelRatio;
            double pixelToFreqRatio;
            ImGuiWindow* window;
        };

        Event<FFTRedrawArgs> onFFTRedraw;

        struct InputHandlerArgs {
            ImVec2 fftRectMin;
            ImVec2 fftRectMax;
            ImVec2 freqScaleRectMin;
            ImVec2 freqScaleRectMax;
            ImVec2 waterfallRectMin;
            ImVec2 waterfallRectMax;
            double lowFreq;
            double highFreq;
            double freqToPixelRatio;
            double pixelToFreqRatio;
        };

        bool inputHandled = false;
        bool VFOMoveSingleClick = false;
        Event<InputHandlerArgs> onInputProcess;

        enum {
            REF_LOWER,
            REF_CENTER,
            REF_UPPER,
            _REF_COUNT
        };

        enum {
            BANDPLAN_POS_BOTTOM,
            BANDPLAN_POS_TOP,
            _BANDPLAN_POS_COUNT
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
        void updateAllVFOs(bool checkRedrawRequired = false);
        bool calculateVFOSignalInfo(float* fftLine, WaterfallVFO* vfo, float& strength, float& snr);

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
        ImVec2 wfMin;
        ImVec2 wfMax;

        ImGuiWindow* window;

        GLuint textureId;

        std::recursive_mutex buf_mtx;

        float vRange;

        int maxVSteps;
        int maxHSteps;

        int dataWidth;           // Width of the FFT and waterfall
        int fftHeight;           // Height of the fft graph
        int waterfallHeight = 0; // Height of the waterfall

        double viewBandwidth;
        double viewOffset;

        double lowerFreq;
        double upperFreq;
        double range;

        float lastDrag;

        int vfoRef = REF_CENTER;

        // Absolute values
        double centerFreq;
        double wholeBandwidth;

        // Ranges
        float fftMin;
        float fftMax;
        float waterfallMin;
        float waterfallMax;

        //std::vector<std::vector<float>> rawFFTs;
        int rawFFTSize;
        float* rawFFTs = NULL;
        float* latestFFT;
        int currentFFTLine = 0;
        int fftLines = 0;

        uint32_t* waterfallFb;

        bool draggingFW = false;
        int FFTAreaHeight;
        int newFFTAreaHeight;

        bool waterfallVisible = true;
        bool bandplanVisible = false;

        bool _fastFFT = true;
        bool _fullUpdate = true;

        int bandPlanPos = BANDPLAN_POS_BOTTOM;

        // UI Select elements
        bool fftResizeSelect = false;
        bool freqScaleSelect = false;
        bool vfoSelect = false;
        bool vfoBorderSelect = false;
        WaterfallVFO* relatedVfo = NULL;
        ImVec2 mouseDownPos;

        ImVec2 lastMousePos;
    };
};