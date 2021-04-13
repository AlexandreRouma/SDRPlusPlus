#include <gui/widgets/waterfall.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <GL/glew.h>
#include <imutils.h>
#include <algorithm>
#include <volk/volk.h>

#include <spdlog/spdlog.h>

float DEFAULT_COLOR_MAP[][3] = {
    {0x00, 0x00, 0x20},
    {0x00, 0x00, 0x30},
    {0x00, 0x00, 0x50},
    {0x00, 0x00, 0x91},
    {0x1E, 0x90, 0xFF},
    {0xFF, 0xFF, 0xFF},
    {0xFF, 0xFF, 0x00},
    {0xFE, 0x6D, 0x16},
    {0xFF, 0x00, 0x00},
    {0xC6, 0x00, 0x00},
    {0x9F, 0x00, 0x00},
    {0x75, 0x00, 0x00},
    {0x4A, 0x00, 0x00}
};

inline void doZoom(int offset, int width, int outWidth, float* data, float* out, bool fast) {
    // NOTE: REMOVE THAT SHIT, IT'S JUST A HACKY FIX
    if (offset < 0) {
        offset = 0;
    }
    if (width > 65535) {
        width = 65535;
    }

    float factor = (float)width / (float)outWidth;

    if (fast) {
        for (int i = 0; i < outWidth; i++) {
            out[i] = data[(int)(offset + ((float)i * factor))];
        }
    }
    else {
        float sFactor = ceilf(factor);
        float id = offset;
        float val, maxVal;
        uint32_t maxId;
        for (int i = 0; i < outWidth; i++) {
            maxVal = -INFINITY;
            for (int j = 0; j < sFactor; j++) {
                if (data[(int)id + j] > maxVal) { maxVal = data[(int)id + j]; }
            }
            out[i] = maxVal;
            id += factor;
        }
    }
}

// TODO: Fix this hacky BS

double freq_ranges[] = {
        1.0, 2.0, 2.5, 5.0,
        10.0, 20.0, 25.0, 50.0,
        100.0, 200.0, 250.0, 500.0,
        1000.0, 2000.0, 2500.0, 5000.0,
        10000.0, 20000.0, 25000.0, 50000.0,
        100000.0, 200000.0, 250000.0, 500000.0,
        1000000.0, 2000000.0, 2500000.0, 5000000.0,
        10000000.0, 20000000.0, 25000000.0, 50000000.0
};

double findBestRange(double bandwidth, int maxSteps) {
    for (int i = 0; i < 32; i++) {
        if (bandwidth / freq_ranges[i] < (double)maxSteps) {
            return freq_ranges[i];
        }
    }
    return 50000000.0;
}

void printAndScale(double freq, char* buf) {
    double freqAbs = fabs(freq);
    if (freqAbs < 1000) {
        sprintf(buf, "%.6g", freq);
    }
    else if (freqAbs < 1000000) {
        sprintf(buf, "%.6lgK", freq / 1000.0);
    }
    else if (freqAbs < 1000000000) {
        sprintf(buf, "%.6lgM", freq / 1000000.0);
    }
    else if (freqAbs < 1000000000000) {
        sprintf(buf, "%.6lgG", freq / 1000000000.0);
    }
    // for (int i = strlen(buf) - 2; i >= 0; i--) {
    //     if (buf[i] != '0') {
    //         if (buf[i] == '.') {
    //             i--;
    //         }
    //         char scale = buf[strlen(buf) - 1];
    //         buf[i + 1] = scale;
    //         buf[i + 2] = 0;
    //         return;
    //     }
    // }
}

namespace ImGui {
    WaterFall::WaterFall() {
        fftMin = -70.0;
        fftMax = 0.0;
        waterfallMin = -70.0;
        waterfallMax = 0.0;
        FFTAreaHeight = 300;
        newFFTAreaHeight = FFTAreaHeight;
        fftHeight = FFTAreaHeight - 50;
        dataWidth = 600;
        lastWidgetPos.x = 0;
        lastWidgetPos.y = 0;
        lastWidgetSize.x = 0;
        lastWidgetSize.y = 0;
        latestFFT = new float[1];
        waterfallFb = new uint32_t[1];

        viewBandwidth = 1.0;
        wholeBandwidth = 1.0;

        updatePallette(DEFAULT_COLOR_MAP, 13);
    }

    void WaterFall::init() {
        glGenTextures(1, &textureId);
    }

    void WaterFall::drawFFT() {
        // Calculate scaling factor
        float startLine = floorf(fftMax / vRange) * vRange;
        float vertRange = fftMax - fftMin;
        float scaleFactor = fftHeight / vertRange;
        char buf[100];

        ImU32 trace = ImGui::GetColorU32(ImGuiCol_PlotLines);
        ImU32 shadow = ImGui::GetColorU32(ImGuiCol_PlotLines, 0.2);

        // Vertical scale
        for (float line = startLine; line > fftMin; line -= vRange) {
            float yPos = widgetPos.y + fftHeight + 10 - ((line - fftMin) * scaleFactor);
            window->DrawList->AddLine(ImVec2(roundf(widgetPos.x + 50), roundf(yPos)), 
                                    ImVec2(roundf(widgetPos.x + dataWidth + 50), roundf(yPos)),
                                    IM_COL32(50, 50, 50, 255), 1.0);
            sprintf(buf, "%d", (int)line);
            ImVec2 txtSz = ImGui::CalcTextSize(buf);
            window->DrawList->AddText(ImVec2(widgetPos.x + 40 - txtSz.x, roundf(yPos - (txtSz.y / 2.0))), IM_COL32( 255, 255, 255, 255 ), buf);
        }

        // Horizontal scale
        double startFreq = ceilf(lowerFreq / range) * range;
        double horizScale = (double)dataWidth / viewBandwidth;
        for (double freq = startFreq; freq < upperFreq; freq += range) {
            double xPos = widgetPos.x + 50 + ((freq - lowerFreq) * horizScale);
            window->DrawList->AddLine(ImVec2(roundf(xPos), widgetPos.y + 10), 
                                    ImVec2(roundf(xPos), widgetPos.y + fftHeight + 10),
                                    IM_COL32(50, 50, 50, 255), 1.0);
            window->DrawList->AddLine(ImVec2(roundf(xPos), widgetPos.y + fftHeight + 10), 
                                    ImVec2(roundf(xPos), widgetPos.y + fftHeight + 17),
                                    IM_COL32(255, 255, 255, 255), 1.0);
            printAndScale(freq, buf);
            ImVec2 txtSz = ImGui::CalcTextSize(buf);
            window->DrawList->AddText(ImVec2(roundf(xPos - (txtSz.x / 2.0)), widgetPos.y + fftHeight + 10 + txtSz.y), IM_COL32( 255, 255, 255, 255 ), buf);
        }

        // Data
        if (latestFFT != NULL && fftLines != 0) {
             for (int i = 1; i < dataWidth; i++) {
                double aPos = widgetPos.y + fftHeight + 10 - ((latestFFT[i - 1] - fftMin) * scaleFactor);
                double bPos = widgetPos.y + fftHeight + 10 - ((latestFFT[i] - fftMin) * scaleFactor);
                aPos = std::clamp<double>(aPos, widgetPos.y + 10, widgetPos.y + fftHeight + 10);
                bPos = std::clamp<double>(bPos, widgetPos.y + 10, widgetPos.y + fftHeight + 10);
                window->DrawList->AddLine(ImVec2(widgetPos.x + 49 + i, roundf(aPos)), 
                                        ImVec2(widgetPos.x + 50 + i, roundf(bPos)), trace, 1.0);
                window->DrawList->AddLine(ImVec2(widgetPos.x + 50 + i, roundf(bPos)), 
                                        ImVec2(widgetPos.x + 50 + i, widgetPos.y + fftHeight + 10), shadow, 1.0);
            }
        }

        // X Axis
        window->DrawList->AddLine(ImVec2(widgetPos.x + 50, widgetPos.y + fftHeight + 10), 
                                    ImVec2(widgetPos.x + dataWidth + 50, widgetPos.y + fftHeight + 10),
                                    IM_COL32(255, 255, 255, 255), 1.0);
        // Y Axis
        window->DrawList->AddLine(ImVec2(widgetPos.x + 50, widgetPos.y + 9), 
                                    ImVec2(widgetPos.x + 50, widgetPos.y + fftHeight + 9),
                                    IM_COL32(255, 255, 255, 255), 1.0);

        
    }

    void WaterFall::drawWaterfall() {
        if (waterfallUpdate) {
            waterfallUpdate = false;
            updateWaterfallTexture();
        }
        window->DrawList->AddImage((void*)(intptr_t)textureId, wfMin, wfMax);
        ImVec2 mPos = ImGui::GetMousePos();

        if (IS_IN_AREA(mPos, wfMin, wfMax)) {
            for (auto const& [name, vfo] : vfos) {
                window->DrawList->AddRectFilled(vfo->wfRectMin, vfo->wfRectMax, IM_COL32(255, 255, 255, 50));
                window->DrawList->AddLine(vfo->wfLineMin, vfo->wfLineMax, (name == selectedVFO) ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 255, 0, 255));
            }
        }
    }

    void WaterFall::drawVFOs() {
        for (auto const& [name, vfo] : vfos) {
            if (vfo->redrawRequired) {
                vfo->redrawRequired = false;
                vfo->updateDrawingVars(viewBandwidth, dataWidth, viewOffset, widgetPos, fftHeight);
                vfo->wfRectMin = ImVec2(vfo->rectMin.x, wfMin.y);
                vfo->wfRectMax = ImVec2(vfo->rectMax.x, wfMax.y);
                vfo->wfLineMin = ImVec2(vfo->lineMin.x, wfMin.y);
                vfo->wfLineMax = ImVec2(vfo->lineMax.x, wfMax.y);
            }
            vfo->draw(window, name == selectedVFO);
        }
    }

    void WaterFall::selectFirstVFO() {
        bool available = false;
        for (auto const& [name, vfo] : vfos) {
            available = true;
            selectedVFO = name;
            return;
        }
        if (!available) {
            selectedVFO = "";
        }
    }

    void WaterFall::processInputs() {
        WaterfallVFO* vfo = NULL;
        if (selectedVFO != "") {
            vfo = vfos[selectedVFO];
        }
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
        ImVec2 dragOrigin(mousePos.x - drag.x, mousePos.y - drag.y);

        bool mouseHovered, mouseHeld;
        bool mouseClicked = ImGui::ButtonBehavior(ImRect(fftAreaMin, fftAreaMax), GetID("WaterfallID"), &mouseHovered, &mouseHeld, 
                                                ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_PressedOnClick);

        mouseClicked |= ImGui::ButtonBehavior(ImRect(wfMin, wfMax), GetID("WaterfallID2"), &mouseHovered, &mouseHeld, 
                                                ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_PressedOnClick);
        
        bool draging = ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::IsWindowFocused();
        bool mouseInFreq = IS_IN_AREA(dragOrigin, freqAreaMin, freqAreaMax);
        bool mouseInFFT = IS_IN_AREA(dragOrigin, fftAreaMin, fftAreaMax);
        bool mouseInWaterfall = IS_IN_AREA(dragOrigin, wfMin, wfMax);
        

        // If mouse was clicked on a VFO, select VFO and return
        // If mouse was clicked but not on a VFO, move selected VFO to position
        if (mouseClicked) {
            for (auto const& [name, _vfo] : vfos) {
                if (name == selectedVFO) {
                    continue;
                }
                if (IS_IN_AREA(mousePos, _vfo->rectMin, _vfo->rectMax) || IS_IN_AREA(mousePos, _vfo->wfRectMin, _vfo->wfRectMax)) {
                    selectedVFO = name;
                    selectedVFOChanged = true;
                    return;
                }
            }
            if (vfo != NULL) {
                int refCenter = mousePos.x - (widgetPos.x + 50);
                if (refCenter >= 0 && refCenter < dataWidth) {
                    double off = ((((double)refCenter / ((double)dataWidth / 2.0)) - 1.0) * (viewBandwidth / 2.0)) + viewOffset;
                    off += centerFreq;
                    off = (round(off / vfo->snapInterval) * vfo->snapInterval) - centerFreq;
                    vfo->setOffset(off);
                }
            }
        }

        // Draging VFO
        if (draging && (mouseInFFT || mouseInWaterfall)) {
            int refCenter = mousePos.x - (widgetPos.x + 50);
            if (refCenter >= 0 && refCenter < dataWidth && mousePos.y > widgetPos.y && mousePos.y < (widgetPos.y + widgetSize.y) && vfo != NULL) {
                double off = ((((double)refCenter / ((double)dataWidth / 2.0)) - 1.0) * (viewBandwidth / 2.0)) + viewOffset;
                off += centerFreq;
                off = (round(off / vfo->snapInterval) * vfo->snapInterval) - centerFreq;
                vfo->setOffset(off);
            }
        } 

        // Draging frequency scale
        if (draging && mouseInFreq) {
            double deltax = drag.x - lastDrag;
            lastDrag = drag.x;
            double viewDelta = deltax * (viewBandwidth / (double)dataWidth);

            viewOffset -= viewDelta;

            if (viewOffset + (viewBandwidth / 2.0) > wholeBandwidth / 2.0) {
                double freqOffset = (viewOffset + (viewBandwidth / 2.0)) - (wholeBandwidth / 2.0);
                viewOffset = (wholeBandwidth / 2.0) - (viewBandwidth / 2.0);
                centerFreq += freqOffset;
                centerFreqMoved = true;
            }
            if (viewOffset - (viewBandwidth / 2.0) < -(wholeBandwidth / 2.0)) {
                double freqOffset = (viewOffset - (viewBandwidth / 2.0)) + (wholeBandwidth / 2.0);
                viewOffset = (viewBandwidth / 2.0) - (wholeBandwidth / 2.0);
                centerFreq += freqOffset;
                centerFreqMoved = true;
            }

            lowerFreq = (centerFreq + viewOffset) - (viewBandwidth / 2.0);
            upperFreq = (centerFreq + viewOffset) + (viewBandwidth / 2.0);

            if (viewBandwidth != wholeBandwidth) {
                updateAllVFOs();
                if (_fullUpdate) { updateWaterfallFb(); };
            }
        }
        else {
            lastDrag = 0;
        }
    }

    void WaterFall::setFastFFT(bool fastFFT) {
        std::lock_guard<std::mutex> lck(buf_mtx);
        _fastFFT = fastFFT;
    }

    void WaterFall::updateWaterfallFb() {
        if (!waterfallVisible || rawFFTs == NULL) {
            return;
        }
        double offsetRatio = viewOffset / (wholeBandwidth / 2.0);
        int drawDataSize;
        int drawDataStart;
        // TODO: Maybe put on the stack for faster alloc?
        float* tempData = new float[dataWidth];
        float pixel;
        float dataRange = waterfallMax - waterfallMin;
        int count = std::min<float>(waterfallHeight, fftLines);
        if (rawFFTs != NULL) {
            for (int i = 0; i < count; i++) {
                drawDataSize = (viewBandwidth / wholeBandwidth) * rawFFTSize;
                drawDataStart = (((double)rawFFTSize / 2.0) * (offsetRatio + 1)) - (drawDataSize / 2);
                doZoom(drawDataStart, drawDataSize, dataWidth, &rawFFTs[((i + currentFFTLine) % waterfallHeight) * rawFFTSize], tempData, _fastFFT);
                for (int j = 0; j < dataWidth; j++) {
                    pixel = (std::clamp<float>(tempData[j], waterfallMin, waterfallMax) - waterfallMin) / dataRange;
                    waterfallFb[(i * dataWidth) + j] = waterfallPallet[(int)(pixel * (WATERFALL_RESOLUTION - 1))];
                }
            }
        }
        delete[] tempData;
        waterfallUpdate = true;
    }

    void WaterFall::drawBandPlan() {
        int count = bandplan->bands.size();
        double horizScale = (double)dataWidth / viewBandwidth;
        double start, end, center, aPos, bPos, cPos, width;
        ImVec2 txtSz;
        bool startVis, endVis;
        uint32_t color, colorTrans;

        float height = ImGui::CalcTextSize("0").y * 2.5f;
        float bpBottom;

        if (bandPlanPos == BANDPLAN_POS_BOTTOM) {
            bpBottom = widgetPos.y + fftHeight + 10;
        }
        else {
            bpBottom = widgetPos.y + height + 10;
        }
        

        for (int i = 0; i < count; i++) {
            start = bandplan->bands[i].start;
            end = bandplan->bands[i].end;
            if (start < lowerFreq && end < lowerFreq) {
                continue;
            }
            if (start > upperFreq && end > upperFreq) {
                continue;
            }
            startVis = (start > lowerFreq);
            endVis = (end < upperFreq);
            start = std::clamp<double>(start, lowerFreq, upperFreq);
            end = std::clamp<double>(end, lowerFreq, upperFreq);
            center = (start + end) / 2.0;
            aPos = widgetPos.x + 50 + ((start - lowerFreq) * horizScale);
            bPos = widgetPos.x + 50 + ((end - lowerFreq) * horizScale);
            cPos = widgetPos.x + 50 + ((center - lowerFreq) * horizScale);
            width = bPos - aPos;
            txtSz = ImGui::CalcTextSize(bandplan->bands[i].name.c_str());
            if (bandplan::colorTable.find(bandplan->bands[i].type.c_str()) != bandplan::colorTable.end()) {
                color = bandplan::colorTable[bandplan->bands[i].type].colorValue;
                colorTrans = bandplan::colorTable[bandplan->bands[i].type].transColorValue;
            }
            else {
                color = IM_COL32(255, 255, 255, 255);
                colorTrans = IM_COL32(255, 255, 255, 100);
            }
            if (aPos <= widgetPos.x + 50) {
                aPos = widgetPos.x + 51;
            }
            if (bPos <= widgetPos.x + 50) {
                bPos = widgetPos.x + 51;
            }
            if (width >= 1.0) {
                window->DrawList->AddRectFilled(ImVec2(roundf(aPos), bpBottom - height), 
                                        ImVec2(roundf(bPos), bpBottom), colorTrans);
                if (startVis) {
                    window->DrawList->AddLine(ImVec2(roundf(aPos), bpBottom - height - 1), 
                                        ImVec2(roundf(aPos), bpBottom - 1), color);
                }
                if (endVis) {
                    window->DrawList->AddLine(ImVec2(roundf(bPos), bpBottom - height - 1), 
                                        ImVec2(roundf(bPos), bpBottom - 1), color);
                }
            }
            if (txtSz.x <= width) {
                window->DrawList->AddText(ImVec2(cPos - (txtSz.x / 2.0), bpBottom - (height / 2.0f) - (txtSz.y / 2.0f)), 
                                    IM_COL32(255, 255, 255, 255), bandplan->bands[i].name.c_str());
            }
        }
    }

    void WaterFall::updateWaterfallTexture() {
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dataWidth, waterfallHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, (uint8_t*)waterfallFb);
    }

    void WaterFall::onPositionChange() {
        // Nothing to see here...
    }

    void WaterFall::onResize() {
        // return if widget is too small
        if (widgetSize.x < 100 || widgetSize.y < 100) {
            return;
        }

        int lastWaterfallHeight = waterfallHeight;

        if (waterfallVisible) {
            FFTAreaHeight = std::min<int>(FFTAreaHeight, widgetSize.y - 50);
            fftHeight = FFTAreaHeight - 50;
            waterfallHeight = widgetSize.y - fftHeight - 52;
        }
        else {
            fftHeight =  widgetSize.y - 50;
        }
        dataWidth = widgetSize.x - 60.0;

        if (waterfallVisible) {
            // Raw FFT resize
            fftLines = std::min<int>(fftLines, waterfallHeight) - 1;
            if (rawFFTs != NULL) {
                if (currentFFTLine != 0) {
                    float* tempWF = new float[currentFFTLine * rawFFTSize];
                    int moveCount = lastWaterfallHeight - currentFFTLine;
                    memcpy(tempWF, rawFFTs, currentFFTLine * rawFFTSize * sizeof(float));
                    memmove(rawFFTs, &rawFFTs[currentFFTLine * rawFFTSize], moveCount * rawFFTSize * sizeof(float));
                    memcpy(&rawFFTs[moveCount * rawFFTSize], tempWF, currentFFTLine * rawFFTSize * sizeof(float));
                    delete[] tempWF;
                }
                currentFFTLine = 0;
                rawFFTs = (float*)realloc(rawFFTs, waterfallHeight * rawFFTSize * sizeof(float));
            }
            else {
                rawFFTs = (float*)malloc(waterfallHeight * rawFFTSize * sizeof(float));
            }
            // ==============
        }

        if (latestFFT != NULL) {
            delete[] latestFFT;
        }
        latestFFT = new float[dataWidth];

        if (waterfallVisible) {
            delete[] waterfallFb;
            waterfallFb = new uint32_t[dataWidth * waterfallHeight];
            memset(waterfallFb, 0, dataWidth * waterfallHeight * sizeof(uint32_t));
        }
        for (int i = 0; i < dataWidth; i++) {
            latestFFT[i] = -1000.0; // Hide everything
        }

        fftAreaMin = ImVec2(widgetPos.x + 50, widgetPos.y + 9);
        fftAreaMax = ImVec2(widgetPos.x + dataWidth + 50, widgetPos.y + fftHeight + 10);
        freqAreaMin = ImVec2(widgetPos.x + 50, widgetPos.y + fftHeight + 11);
        freqAreaMax = ImVec2(widgetPos.x + dataWidth + 50, widgetPos.y + fftHeight + 50);
        wfMin = ImVec2(widgetPos.x + 50, widgetPos.y + fftHeight + 51);
        wfMax = ImVec2(widgetPos.x + 50 + dataWidth, widgetPos.y + fftHeight + 51 + waterfallHeight);

        maxHSteps = dataWidth / (ImGui::CalcTextSize("000.000").x + 10);
        maxVSteps = fftHeight / (ImGui::CalcTextSize("000.000").y);

        range = findBestRange(viewBandwidth, maxHSteps);
        vRange = findBestRange(fftMax - fftMin, maxVSteps);

        updateWaterfallFb();
        updateAllVFOs();
    }

    void WaterFall::draw() {
        buf_mtx.lock();
        window = GetCurrentWindow();

        widgetPos = ImGui::GetWindowContentRegionMin();
        widgetEndPos = ImGui::GetWindowContentRegionMax();
        widgetPos.x += window->Pos.x;
        widgetPos.y += window->Pos.y;
        widgetEndPos.x += window->Pos.x - 4; // Padding
        widgetEndPos.y += window->Pos.y;
        widgetSize = ImVec2(widgetEndPos.x - widgetPos.x, widgetEndPos.y - widgetPos.y);

        if (selectedVFO == "" && vfos.size() > 0) {
            selectFirstVFO();
        }

        if (widgetPos.x != lastWidgetPos.x || widgetPos.y != lastWidgetPos.y) {
            lastWidgetPos = widgetPos;
            onPositionChange();
        }
        if (widgetSize.x != lastWidgetSize.x || widgetSize.y != lastWidgetSize.y) {
            lastWidgetSize = widgetSize;
            onResize();
        }

        window->DrawList->AddRectFilled(widgetPos, widgetEndPos, IM_COL32( 0, 0, 0, 255 ));
        window->DrawList->AddRect(widgetPos, widgetEndPos, IM_COL32( 50, 50, 50, 255 ));
        window->DrawList->AddLine(ImVec2(widgetPos.x, widgetPos.y + fftHeight + 50), ImVec2(widgetPos.x + widgetSize.x, widgetPos.y + fftHeight + 50), IM_COL32(50, 50, 50, 255), 1.0);

        processInputs();
        
        drawFFT();
        if (waterfallVisible) {
            drawWaterfall();
        }
        drawVFOs();
        if (bandplan != NULL && bandplanVisible) {
            drawBandPlan();
        }

        if (!waterfallVisible) {
            buf_mtx.unlock();
            return;
        }

        // Handle fft resize
        ImVec2 winSize = ImGui::GetWindowSize();
        ImVec2 mousePos = ImGui::GetMousePos();
        mousePos.x -= widgetPos.x;
        mousePos.y -= widgetPos.y;
        bool click = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        bool down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        if (draggingFW) {
            newFFTAreaHeight = mousePos.y;
            newFFTAreaHeight = std::clamp<float>(newFFTAreaHeight, 150, widgetSize.y - 50);
            ImGui::GetForegroundDrawList()->AddLine(ImVec2(widgetPos.x, newFFTAreaHeight + widgetPos.y), ImVec2(widgetEndPos.x, newFFTAreaHeight + widgetPos.y), 
                                                    ImGui::GetColorU32(ImGuiCol_SeparatorActive));
        }
        if (mousePos.y >= newFFTAreaHeight - 2 && mousePos.y <= newFFTAreaHeight + 2 && mousePos.x > 0 && mousePos.x < widgetSize.x) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            if (click) {
                draggingFW = true;
            }
        }
        if(!down && draggingFW) {
            draggingFW = false;
            FFTAreaHeight = newFFTAreaHeight;
            onResize();
        }

        buf_mtx.unlock();
    }

    float* WaterFall::getFFTBuffer() {
        if (rawFFTs == NULL) { return NULL; }
        buf_mtx.lock();
        if (waterfallVisible) {
            currentFFTLine--;
            fftLines++;
            currentFFTLine = ((currentFFTLine + waterfallHeight) % waterfallHeight);
            fftLines = std::min<float>(fftLines, waterfallHeight);
            return &rawFFTs[currentFFTLine * rawFFTSize];
        }
        return rawFFTs;
    }

    void WaterFall::pushFFT() {
        if (rawFFTs == NULL) { return; }
        double offsetRatio = viewOffset / (wholeBandwidth / 2.0);
        int drawDataSize = (viewBandwidth / wholeBandwidth) * rawFFTSize;
        int drawDataStart = (((double)rawFFTSize / 2.0) * (offsetRatio + 1)) - (drawDataSize / 2);
        
        // If in fast mode, apply IIR filtering
        float* buf = &rawFFTs[currentFFTLine * rawFFTSize];
        if (_fastFFT) {
            float last = buf[0];
            for (int i = 0; i < rawFFTSize; i++) {
                last = (buf[i] * 0.1f) + (last * 0.9f);
                buf[i] = last;
            }
        }

        if (waterfallVisible) {
            doZoom(drawDataStart, drawDataSize, dataWidth, &rawFFTs[currentFFTLine * rawFFTSize], latestFFT, _fastFFT);
            memmove(&waterfallFb[dataWidth], waterfallFb, dataWidth * (waterfallHeight - 1) * sizeof(uint32_t));
            float pixel;
            float dataRange = waterfallMax - waterfallMin;
            for (int j = 0; j < dataWidth; j++) {
                pixel = (std::clamp<float>(latestFFT[j], waterfallMin, waterfallMax) - waterfallMin) / dataRange;
                int id = (int)(pixel * (WATERFALL_RESOLUTION - 1));
                waterfallFb[j] = waterfallPallet[id];
            }
            waterfallUpdate = true;
        }
        else {
            doZoom(drawDataStart, drawDataSize, dataWidth, rawFFTs, latestFFT, _fastFFT);
            fftLines = 1;
        }
        
        buf_mtx.unlock();
    }

    void WaterFall::updatePallette(float colors[][3], int colorCount) {
        std::lock_guard<std::mutex> lck(buf_mtx);
        for (int i = 0; i < WATERFALL_RESOLUTION; i++) {
            int lowerId = floorf(((float)i / (float)WATERFALL_RESOLUTION) * colorCount);
            int upperId = ceilf(((float)i / (float)WATERFALL_RESOLUTION) * colorCount);
            lowerId = std::clamp<int>(lowerId, 0, colorCount - 1);
            upperId = std::clamp<int>(upperId, 0, colorCount - 1);
            float ratio = (((float)i / (float)WATERFALL_RESOLUTION) * colorCount) - lowerId;
            float r = (colors[lowerId][0] * (1.0 - ratio)) + (colors[upperId][0] * (ratio));
            float g = (colors[lowerId][1] * (1.0 - ratio)) + (colors[upperId][1] * (ratio));
            float b = (colors[lowerId][2] * (1.0 - ratio)) + (colors[upperId][2] * (ratio));
            waterfallPallet[i] = ((uint32_t)255 << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
        }
        updateWaterfallFb();
    }

    void WaterFall::updatePalletteFromArray(float* colors, int colorCount) {
        std::lock_guard<std::mutex> lck(buf_mtx);
        for (int i = 0; i < WATERFALL_RESOLUTION; i++) {
            int lowerId = floorf(((float)i / (float)WATERFALL_RESOLUTION) * colorCount);
            int upperId = ceilf(((float)i / (float)WATERFALL_RESOLUTION) * colorCount);
            lowerId = std::clamp<int>(lowerId, 0, colorCount - 1);
            upperId = std::clamp<int>(upperId, 0, colorCount - 1);
            float ratio = (((float)i / (float)WATERFALL_RESOLUTION) * colorCount) - lowerId;
            float r = (colors[(lowerId * 3) + 0] * (1.0 - ratio)) + (colors[(upperId * 3) + 0] * (ratio));
            float g = (colors[(lowerId * 3) + 1] * (1.0 - ratio)) + (colors[(upperId * 3) + 1] * (ratio));
            float b = (colors[(lowerId * 3) + 2] * (1.0 - ratio)) + (colors[(upperId * 3) + 2] * (ratio));
            waterfallPallet[i] = ((uint32_t)255 << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
        }
        updateWaterfallFb();
    }

    void WaterFall::autoRange() {
        float min = INFINITY;
        float max = -INFINITY;
        for (int i = 0; i < dataWidth; i++) {
            if (latestFFT[i] < min) {
                min = latestFFT[i];
            }
            if (latestFFT[i] > max) {
                max = latestFFT[i];
            }
        }
        fftMin = min - 5;
        fftMax = max + 5;
    }

    void WaterFall::setCenterFrequency(double freq) {
        centerFreq = freq;
        lowerFreq = (centerFreq + viewOffset) - (viewBandwidth / 2.0);
        upperFreq = (centerFreq + viewOffset) + (viewBandwidth / 2.0);
        updateAllVFOs();
    }

    double WaterFall::getCenterFrequency() {
        return centerFreq;
    }

    void WaterFall::setBandwidth(double bandWidth) {
        double currentRatio = viewBandwidth / wholeBandwidth;
        wholeBandwidth = bandWidth;
        setViewBandwidth(bandWidth * currentRatio);
        for (auto const& [name, vfo] : vfos) {
            if (vfo->lowerOffset < -(bandWidth / 2)) {
                vfo->setCenterOffset(-(bandWidth / 2));
            }
            if (vfo->upperOffset > (bandWidth / 2)) {
                vfo->setCenterOffset(bandWidth / 2);
            }
        }
        updateAllVFOs();
    }

    double WaterFall::getBandwidth() {
        return wholeBandwidth;
    }

    void WaterFall::setViewBandwidth(double bandWidth) {
        std::lock_guard<std::mutex> lck(buf_mtx);
        if (bandWidth == viewBandwidth) {
            return;
        }
        if (abs(viewOffset) + (bandWidth / 2.0) > wholeBandwidth / 2.0) {
            if (viewOffset < 0) {
                viewOffset = (bandWidth / 2.0) - (wholeBandwidth / 2.0);
            }
            else {
                viewOffset = (wholeBandwidth / 2.0) - (bandWidth / 2.0);
            }
        }
        viewBandwidth = bandWidth;
        lowerFreq = (centerFreq + viewOffset) - (viewBandwidth / 2.0);
        upperFreq = (centerFreq + viewOffset) + (viewBandwidth / 2.0);
        range = findBestRange(bandWidth, maxHSteps);
        if (_fullUpdate) { updateWaterfallFb(); };
        updateAllVFOs();
    }

    double WaterFall::getViewBandwidth() {
        return viewBandwidth;
    }

    void WaterFall::setViewOffset(double offset) {
        std::lock_guard<std::mutex> lck(buf_mtx);
        if (offset == viewOffset) {
            return;
        }
        if (offset - (viewBandwidth / 2.0) < -(wholeBandwidth / 2.0)) {
            offset = (viewBandwidth / 2.0) - (wholeBandwidth / 2.0);
        }
        if (offset + (viewBandwidth / 2.0) > (wholeBandwidth / 2.0)) {
            offset = (wholeBandwidth / 2.0) - (viewBandwidth / 2.0);
        }
        viewOffset = offset;
        lowerFreq = (centerFreq + viewOffset) - (viewBandwidth / 2.0);
        upperFreq = (centerFreq + viewOffset) + (viewBandwidth / 2.0);
        if (_fullUpdate) { updateWaterfallFb(); };
        updateAllVFOs();
    }

    double WaterFall::getViewOffset() {
        return viewOffset;
    }
    
    void WaterFall::setFFTMin(float min) {
        fftMin = min;
        vRange = findBestRange(fftMax - fftMin, maxVSteps);
    }

    float WaterFall::getFFTMin() {
        return fftMin;
    }

    void WaterFall::setFFTMax(float max) {
        fftMax = max;
        vRange = findBestRange(fftMax - fftMin, maxVSteps);
    }

    float WaterFall::getFFTMax() {
        return fftMax;
    }

    void WaterFall::setFullWaterfallUpdate(bool fullUpdate) {
        std::lock_guard<std::mutex> lck(buf_mtx);
        _fullUpdate = fullUpdate;
    }

    void WaterFall::setWaterfallMin(float min) {
        std::lock_guard<std::mutex> lck(buf_mtx);
        if (min == waterfallMin) {
            return;
        }
        waterfallMin = min;
        if (_fullUpdate) { updateWaterfallFb(); };
    }

    float WaterFall::getWaterfallMin() {
        return waterfallMin;
    }

    void WaterFall::setWaterfallMax(float max) {
        std::lock_guard<std::mutex> lck(buf_mtx);
        if (max == waterfallMax) {
            return;
        }
        waterfallMax = max;
        if (_fullUpdate) { updateWaterfallFb(); };
    }

    float WaterFall::getWaterfallMax() {
        return waterfallMax;
    }

    void WaterFall::updateAllVFOs() {
        for (auto const& [name, vfo] : vfos) {
            vfo->updateDrawingVars(viewBandwidth, dataWidth, viewOffset, widgetPos, fftHeight);
            vfo->wfRectMin = ImVec2(vfo->rectMin.x, wfMin.y);
            vfo->wfRectMax = ImVec2(vfo->rectMax.x, wfMax.y);
            vfo->wfLineMin = ImVec2(vfo->lineMin.x, wfMin.y);
            vfo->wfLineMax = ImVec2(vfo->lineMax.x, wfMax.y);
        }
    }

    void WaterFall::setRawFFTSize(int size, bool lock) {
        std::lock_guard<std::mutex> lck(buf_mtx);
        rawFFTSize = size;
        if (rawFFTs != NULL) {
            int wfSize = std::max<int>(1, waterfallHeight);
            rawFFTs = (float*)realloc(rawFFTs, rawFFTSize * wfSize * sizeof(float));
        }
        else {
            int wfSize = std::max<int>(1, waterfallHeight);
            rawFFTs = (float*)malloc(rawFFTSize * wfSize * sizeof(float));
        }
        memset(rawFFTs, 0, rawFFTSize * waterfallHeight * sizeof(float));
    }

    void WaterFall::setBandPlanPos(int pos) {
        bandPlanPos = pos;
    }

    void WaterfallVFO::setOffset(double offset) {
        generalOffset = offset;
        if (reference == REF_CENTER) {
            centerOffset = offset;
            lowerOffset = offset - (bandwidth / 2.0);
            upperOffset = offset + (bandwidth / 2.0);
        }
        else if (reference == REF_LOWER) {
            lowerOffset = offset;
            centerOffset = offset + (bandwidth / 2.0);
            upperOffset = offset + bandwidth;
        }
        else if (reference == REF_UPPER) {
            upperOffset = offset;
            centerOffset = offset - (bandwidth / 2.0);
            lowerOffset = offset - bandwidth;
        }
        centerOffsetChanged = true;
        upperOffsetChanged = true;
        lowerOffsetChanged = true;
        redrawRequired = true;
    }

    void WaterfallVFO::setCenterOffset(double offset) {
        if (reference == REF_CENTER) {
            generalOffset = offset;
        }
        else if (reference == REF_LOWER) {
            generalOffset = offset - (bandwidth / 2.0);
        }
        else if (reference == REF_UPPER) {
            generalOffset = offset + (bandwidth / 2.0);
        }
        centerOffset = offset;
        lowerOffset = offset - (bandwidth / 2.0);
        upperOffset = offset + (bandwidth / 2.0);
        centerOffsetChanged = true;
        upperOffsetChanged = true;
        lowerOffsetChanged = true;
        redrawRequired = true;
    }

    void WaterfallVFO::setBandwidth(double bw) {
        if (bandwidth == bw || bw < 0) {
            return;
        }
        bandwidth = bw;
        if (reference == REF_CENTER) {
            lowerOffset = centerOffset - (bandwidth / 2.0);
            upperOffset = centerOffset + (bandwidth / 2.0);
        }
        else if (reference == REF_LOWER) {
            centerOffset = lowerOffset + (bandwidth / 2.0);
            upperOffset = lowerOffset + bandwidth;
            centerOffsetChanged = true;
        }
        else if (reference == REF_UPPER) {
            centerOffset = upperOffset - (bandwidth / 2.0);
            lowerOffset = upperOffset - bandwidth;
            centerOffsetChanged = true;
        }
        redrawRequired = true;
    }

    void WaterfallVFO::setReference(int ref) {
        if (reference == ref || ref < 0 || ref >= _REF_COUNT) {
            return;
        }
        reference = ref;
        setOffset(generalOffset);
        
    }

    void WaterfallVFO::updateDrawingVars(double viewBandwidth, float dataWidth, double viewOffset, ImVec2 widgetPos, int fftHeight) {
        double width = (bandwidth / viewBandwidth) * (double)dataWidth;
        int center = roundf((((centerOffset - viewOffset) / (viewBandwidth / 2.0)) + 1.0) * ((double)dataWidth / 2.0));
        int left = roundf((((lowerOffset - viewOffset) / (viewBandwidth / 2.0)) + 1.0) * ((double)dataWidth / 2.0));
        int right = roundf((((upperOffset - viewOffset) / (viewBandwidth / 2.0)) + 1.0) * ((double)dataWidth / 2.0));

        if (left >= 0 && left < dataWidth && reference == REF_LOWER) {
            lineMin = ImVec2(widgetPos.x + 50 + left, widgetPos.y + 9);
            lineMax = ImVec2(widgetPos.x + 50 + left, widgetPos.y + fftHeight + 9);
            lineVisible = true;
        }
        else if (center >= 0 && center < dataWidth && reference == REF_CENTER) {
            lineMin = ImVec2(widgetPos.x + 50 + center, widgetPos.y + 9);
            lineMax = ImVec2(widgetPos.x + 50 + center, widgetPos.y + fftHeight + 9);
            lineVisible = true;
        }
        else if (right >= 0 && right < dataWidth && reference == REF_UPPER) {
            lineMin = ImVec2(widgetPos.x + 50 + right, widgetPos.y + 9);
            lineMax = ImVec2(widgetPos.x + 50 + right, widgetPos.y + fftHeight + 9);
            lineVisible = true;
        }
        else {
            lineVisible = false;
        }

        left = std::clamp<int>(left, 0, dataWidth - 1);
        right = std::clamp<int>(right, 0, dataWidth - 1);

        rectMin = ImVec2(widgetPos.x + 50 + left, widgetPos.y + 10);
        rectMax = ImVec2(widgetPos.x + 51 + right, widgetPos.y + fftHeight + 10);
    }

    void WaterfallVFO::draw(ImGuiWindow* window, bool selected) {
        window->DrawList->AddRectFilled(rectMin, rectMax, IM_COL32(255, 255, 255, 50));
        if (lineVisible) {
            window->DrawList->AddLine(lineMin, lineMax, selected ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 255, 0, 255));
        }
    };

    void WaterFall::showWaterfall() {
        buf_mtx.lock();
        waterfallVisible = true;
        onResize();
        memset(rawFFTs, 0, waterfallHeight * rawFFTSize * sizeof(float));
        updateWaterfallFb();
        buf_mtx.unlock();
    }

    void WaterFall::hideWaterfall() {
        buf_mtx.lock();
        waterfallVisible = false;
        onResize();
        buf_mtx.unlock();
    }

    void WaterFall::setFFTHeight(int height) {
        FFTAreaHeight = height;
        newFFTAreaHeight = height;
        buf_mtx.lock();
        onResize();
        buf_mtx.unlock();
    }
    
    int WaterFall::getFFTHeight() {
        return FFTAreaHeight;
    }

    void WaterFall::showBandplan() {
        bandplanVisible = true;
    }

    void WaterFall::hideBandplan() {
        bandplanVisible = false;
    }

    void WaterfallVFO::setSnapInterval(double interval) {
        snapInterval = interval;
    }
};

