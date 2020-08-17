#include <waterfall.h>

float COLOR_MAP[][3] = {
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

void doZoom(int offset, int width, int outWidth, std::vector<float> data, float* out) {
    // NOTE: REMOVE THAT SHIT, IT'S JUST A HACKY FIX
    if (offset < 0) {
        offset = 0;
    }
    if (width > 65535) {
        width = 65535;
    }

    float factor = (float)width / (float)outWidth;
    for (int i = 0; i < outWidth; i++) {
        out[i] = data[offset + ((float)i * factor)];
    }
}

float freq_ranges[] = {
        1.0f, 2.0f, 2.5f, 5.0f,
        10.0f, 20.0f, 25.0f, 50.0f,
        100.0f, 200.0f, 250.0f, 500.0f,
        1000.0f, 2000.0f, 2500.0f, 5000.0f,
        10000.0f, 20000.0f, 25000.0f, 50000.0f,
        100000.0f, 200000.0f, 250000.0f, 500000.0f,
        1000000.0f, 2000000.0f, 2500000.0f, 5000000.0f,
        10000000.0f, 20000000.0f, 25000000.0f, 50000000.0f
};

float findBestRange(float bandwidth, int maxSteps) {
    for (int i = 0; i < 32; i++) {
        if (bandwidth / freq_ranges[i] < (float)maxSteps) {
            return freq_ranges[i];
        }
    }
    return 50000000.0f;
}

void printAndScale(float freq, char* buf) {
    if (freq < 1000) {
        sprintf(buf, "%.3f", freq);
    }
    else if (freq < 1000000) {
        sprintf(buf, "%.3fK", freq / 1000.0f);
    }
    else if (freq < 1000000000) {
        sprintf(buf, "%.3fM", freq / 1000000.0f);
    }
    else if (freq < 1000000000000) {
        sprintf(buf, "%.3fG", freq / 1000000000.0f);
    }
    for (int i = strlen(buf) - 2; i >= 0; i--) {
        if (buf[i] != '0') {
            if (buf[i] == '.') {
                i--;
            }
            char scale = buf[strlen(buf) - 1];
            buf[i + 1] = scale;
            buf[i + 2] = 0;
            return;
        }
    }
}

namespace ImGui {
    WaterFall::WaterFall() {
        fftMin = -70.0f;
        fftMax = 0.0f;
        waterfallMin = -70.0f;
        waterfallMax = 0.0f;
        fftHeight = 250;
        dataWidth = 600;
        lastWidgetPos.x = 0;
        lastWidgetPos.y = 0;
        lastWidgetSize.x = 0;
        lastWidgetSize.y = 0;
        latestFFT = new float[1];
        waterfallFb = new uint32_t[1];

        viewBandwidth = 1.0f;
        wholeBandwidth = 1.0f;

        glGenTextures(1, &textureId);

        updatePallette(COLOR_MAP, 13);
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
            window->DrawList->AddLine(ImVec2(widgetPos.x + 50, roundf(yPos)), 
                                    ImVec2(widgetPos.x + dataWidth + 50, roundf(yPos)),
                                    IM_COL32(50, 50, 50, 255), 1.0f);
            sprintf(buf, "%d", (int)line);
            ImVec2 txtSz = ImGui::CalcTextSize(buf);
            window->DrawList->AddText(ImVec2(widgetPos.x + 40 - txtSz.x, roundf(yPos - (txtSz.y / 2))), IM_COL32( 255, 255, 255, 255 ), buf);
        }

        // Horizontal scale
        float startFreq = ceilf(lowerFreq / range) * range;
        float horizScale = (float)dataWidth / viewBandwidth;
        for (float freq = startFreq; freq < upperFreq; freq += range) {
            float xPos = widgetPos.x + 50 + ((freq - lowerFreq) * horizScale);
            window->DrawList->AddLine(ImVec2(roundf(xPos), widgetPos.y + 10), 
                                    ImVec2(roundf(xPos), widgetPos.y + fftHeight + 10),
                                    IM_COL32(50, 50, 50, 255), 1.0f);
            window->DrawList->AddLine(ImVec2(roundf(xPos), widgetPos.y + fftHeight + 10), 
                                    ImVec2(roundf(xPos), widgetPos.y + fftHeight + 17),
                                    IM_COL32(255, 255, 255, 255), 1.0f);
            printAndScale(freq, buf);
            ImVec2 txtSz = ImGui::CalcTextSize(buf);
            window->DrawList->AddText(ImVec2(roundf(xPos - (txtSz.x / 2.0f)), widgetPos.y + fftHeight + 10 + txtSz.y), IM_COL32( 255, 255, 255, 255 ), buf);
        }

        // Data
        for (int i = 1; i < dataWidth; i++) {
            float aPos = widgetPos.y + fftHeight + 10 - ((latestFFT[i - 1] - fftMin) * scaleFactor);
            float bPos = widgetPos.y + fftHeight + 10 - ((latestFFT[i] - fftMin) * scaleFactor);
            if (aPos < fftMin && bPos < fftMin) {
                continue;
            }
            aPos = std::clamp<float>(aPos, widgetPos.y + 10, widgetPos.y + fftHeight + 10);
            bPos = std::clamp<float>(bPos, widgetPos.y + 10, widgetPos.y + fftHeight + 10);
            window->DrawList->AddLine(ImVec2(widgetPos.x + 49 + i, roundf(aPos)), 
                                    ImVec2(widgetPos.x + 50 + i, roundf(bPos)), trace, 1.0f);
            window->DrawList->AddLine(ImVec2(widgetPos.x + 50 + i, roundf(bPos)), 
                                    ImVec2(widgetPos.x + 50 + i, widgetPos.y + fftHeight + 10), shadow, 1.0f);
        }

        // X Axis
        window->DrawList->AddLine(ImVec2(widgetPos.x + 50, widgetPos.y + fftHeight + 10), 
                                    ImVec2(widgetPos.x + dataWidth + 50, widgetPos.y + fftHeight + 10),
                                    IM_COL32(255, 255, 255, 255), 1.0f);
        // Y Axis
        window->DrawList->AddLine(ImVec2(widgetPos.x + 50, widgetPos.y + 9), 
                                    ImVec2(widgetPos.x + 50, widgetPos.y + fftHeight + 9),
                                    IM_COL32(255, 255, 255, 255), 1.0f);

        
    }

    void WaterFall::drawWaterfall() {
        if (waterfallUpdate) {
            waterfallUpdate = false;
            updateWaterfallTexture();
        }
        window->DrawList->AddImage((void*)(intptr_t)textureId, ImVec2(widgetPos.x + 50, widgetPos.y + fftHeight + 51),
                                ImVec2(widgetPos.x + 50 + dataWidth, widgetPos.y + fftHeight + 51 + waterfallHeight));
    }

    void WaterFall::drawVFOs() {
        for (auto const& [name, vfo] : vfos) {
            if (vfo->redrawRequired) {
                vfo->redrawRequired = false;
                vfo->updateDrawingVars(viewBandwidth, dataWidth, viewOffset, widgetPos, fftHeight);
            }
            vfo->draw(window, name == selectedVFO);
        }
    }

    void WaterFall::selectFirstVFO() {
        for (auto const& [name, vfo] : vfos) {
            selectedVFO = name;
            return;
        }
    }

    void WaterFall::processInputs() {
        WaterfallVFO* vfo = vfos[selectedVFO];
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
        ImVec2 dragOrigin(mousePos.x - drag.x, mousePos.y - drag.y);

        bool mouseHovered, mouseHeld;
        bool mouseClicked = ImGui::ButtonBehavior(ImRect(fftAreaMin, fftAreaMax), ImGuiID("WaterfallID"), &mouseHovered, &mouseHeld, 
                                                ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_PressedOnClick);
        
        bool draging = ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::IsWindowFocused();
        bool mouseInFreq = IS_IN_AREA(dragOrigin, freqAreaMin, freqAreaMax);
        bool mouseInFFT = IS_IN_AREA(dragOrigin, fftAreaMin, fftAreaMax);
        

        // If mouse was clicked on a VFO, select VFO and return
        // If mouse was clicked but not on a VFO, move selected VFO to position
        if (mouseClicked) {
            for (auto const& [name, _vfo] : vfos) {
                if (name == selectedVFO) {
                    continue;
                }
                if (IS_IN_AREA(mousePos, _vfo->rectMin, _vfo->rectMax)) {
                    selectedVFO = name;
                    selectedVFOChanged = true;
                    return;
                }
            }
            int refCenter = mousePos.x - (widgetPos.x + 50);
            if (refCenter >= 0 && refCenter < dataWidth && mousePos.y > widgetPos.y && mousePos.y < (widgetPos.y + widgetSize.y)) {
                vfo->setOffset(((((float)refCenter / ((float)dataWidth / 2.0f)) - 1.0f) * (viewBandwidth / 2.0f)) + viewOffset);
            }
        }

        // Draging VFO
        if (draging && mouseInFFT) {
            int refCenter = mousePos.x - (widgetPos.x + 50);
            if (refCenter >= 0 && refCenter < dataWidth && mousePos.y > widgetPos.y && mousePos.y < (widgetPos.y + widgetSize.y)) {
                vfo->setOffset(((((float)refCenter / ((float)dataWidth / 2.0f)) - 1.0f) * (viewBandwidth / 2.0f)) + viewOffset);
            }
        } 

        // Dragon frequency scale
        if (draging && mouseInFreq) {
            float deltax = drag.x - lastDrag;
            lastDrag = drag.x;
            float viewDelta = deltax * (viewBandwidth / (float)dataWidth);

            viewOffset -= viewDelta;

            if (viewOffset + (viewBandwidth / 2.0f) > wholeBandwidth / 2.0f) {
                float freqOffset = (viewOffset + (viewBandwidth / 2.0f)) - (wholeBandwidth / 2.0f);
                viewOffset = (wholeBandwidth / 2.0f) - (viewBandwidth / 2.0f);
                centerFreq += freqOffset;
                centerFreqMoved = true;
            }
            if (viewOffset - (viewBandwidth / 2.0f) < -(wholeBandwidth / 2.0f)) {
                float freqOffset = (viewOffset - (viewBandwidth / 2.0f)) + (wholeBandwidth / 2.0f);
                viewOffset = (viewBandwidth / 2.0f) - (wholeBandwidth / 2.0f);
                centerFreq += freqOffset;
                centerFreqMoved = true;
            }

            lowerFreq = (centerFreq + viewOffset) - (viewBandwidth / 2.0f);
            upperFreq = (centerFreq + viewOffset) + (viewBandwidth / 2.0f);
            updateWaterfallFb();
        }
        else {
            lastDrag = 0;
        }
    }

    void WaterFall::updateWaterfallFb() {
        float offsetRatio = viewOffset / (wholeBandwidth / 2.0f);
        int drawDataSize;
        int drawDataStart;
        int count = std::min<int>(waterfallHeight, rawFFTs.size());
        float* tempData = new float[dataWidth];
        float pixel;
        float dataRange = waterfallMax - waterfallMin;
        int size;
        for (int i = 0; i < count; i++) {
            size = rawFFTs[i].size();
            drawDataSize = (viewBandwidth / wholeBandwidth) * size;
            drawDataStart = (((float)size / 2.0f) * (offsetRatio + 1)) - (drawDataSize / 2);
            doZoom(drawDataStart, drawDataSize, dataWidth, rawFFTs[i], tempData);
            for (int j = 0; j < dataWidth; j++) {
                pixel = (std::clamp<float>(tempData[j], waterfallMin, waterfallMax) - waterfallMin) / dataRange;
                waterfallFb[(i * dataWidth) + j] = waterfallPallet[(int)(pixel * (WATERFALL_RESOLUTION - 1))];
            }
        }
        delete[] tempData;
        waterfallUpdate = true;
    }

    void WaterFall::drawBandPlan() {
        int count = bandplan->bands.size();
        float horizScale = (float)dataWidth / viewBandwidth;
        float start, end, center, aPos, bPos, cPos, width;
        ImVec2 txtSz;
        bool startVis, endVis;
        uint32_t color, colorTrans;
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
            start = std::clamp<float>(start, lowerFreq, upperFreq);
            end = std::clamp<float>(end, lowerFreq, upperFreq);
            center = (start + end) / 2.0f;
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
            if (width >= 1.0f) {
                window->DrawList->AddRectFilled(ImVec2(roundf(aPos), widgetPos.y + fftHeight - 25), 
                                        ImVec2(roundf(bPos), widgetPos.y + fftHeight + 10), colorTrans);
                if (startVis) {
                    window->DrawList->AddLine(ImVec2(roundf(aPos), widgetPos.y + fftHeight - 26), 
                                        ImVec2(roundf(aPos), widgetPos.y + fftHeight + 9), color);
                }
                if (endVis) {
                    window->DrawList->AddLine(ImVec2(roundf(bPos), widgetPos.y + fftHeight - 26), 
                                        ImVec2(roundf(bPos), widgetPos.y + fftHeight + 9), color);
                }
            }
            if (txtSz.x <= width) {
                window->DrawList->AddText(ImVec2(cPos - (txtSz.x / 2.0f), widgetPos.y + fftHeight - 17), 
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
        dataWidth = widgetSize.x - 60.0f;
        waterfallHeight = widgetSize.y - fftHeight - 52;
        delete[] latestFFT;
        delete[] waterfallFb;

        latestFFT = new float[dataWidth];
        waterfallFb = new uint32_t[dataWidth * waterfallHeight];
        memset(waterfallFb, 0, dataWidth * waterfallHeight * sizeof(uint32_t));
        for (int i = 0; i < dataWidth; i++) {
            latestFFT[i] = -1000.0f; // Hide everything
        }

        fftAreaMin = ImVec2(widgetPos.x + 50, widgetPos.y + 9);
        fftAreaMax = ImVec2(widgetPos.x + dataWidth + 50, widgetPos.y + fftHeight + 10);
        freqAreaMin = ImVec2(widgetPos.x + 50, widgetPos.y + fftHeight + 11);
        freqAreaMax = ImVec2(widgetPos.x + dataWidth + 50, widgetPos.y + fftHeight + 50);

        maxHSteps = dataWidth / (ImGui::CalcTextSize("000.000").x + 10);
        maxVSteps = fftHeight / (ImGui::CalcTextSize("000.000").y);

        range = findBestRange(viewBandwidth, maxHSteps);
        vRange = findBestRange(fftMax - fftMin, maxVSteps);
        vRange = 10.0f;

        updateWaterfallFb();
        updateAllVFOs();
    }

    void WaterFall::draw() {
        buf_mtx.lock();
        window = GetCurrentWindow();

        // Fix for weird ImGui bug
        ImVec2 tmpWidgetEndPos = ImGui::GetWindowContentRegionMax();
        if (tmpWidgetEndPos.x < 100 || tmpWidgetEndPos.y < fftHeight + 100) {
            buf_mtx.unlock();
            return;
        }

        widgetPos = ImGui::GetWindowContentRegionMin();
        widgetEndPos = tmpWidgetEndPos;
        widgetPos.x += window->Pos.x;
        widgetPos.y += window->Pos.y;
        widgetEndPos.x += window->Pos.x - 4; // Padding
        widgetEndPos.y += window->Pos.y;
        widgetSize = ImVec2(widgetEndPos.x - widgetPos.x, widgetEndPos.y - widgetPos.y);

        

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
        window->DrawList->AddLine(ImVec2(widgetPos.x, widgetPos.y + fftHeight + 50), ImVec2(widgetPos.x + widgetSize.x, widgetPos.y + fftHeight + 50), IM_COL32(50, 50, 50, 255), 1.0f);

        processInputs();
        
        drawFFT();
        drawWaterfall();
        drawVFOs();
        if (bandplan != NULL) {
            drawBandPlan();
        }

        buf_mtx.unlock();
    }

    void WaterFall::pushFFT(std::vector<float> data, int n) {
        buf_mtx.lock();
        float offsetRatio = viewOffset / (wholeBandwidth / 2.0f);
        int drawDataSize = (viewBandwidth / wholeBandwidth) * data.size();
        int drawDataStart = (((float)data.size() / 2.0f) * (offsetRatio + 1)) - (drawDataSize / 2);
        
        doZoom(drawDataStart, drawDataSize, dataWidth, data, latestFFT);
        rawFFTs.insert(rawFFTs.begin(), data);
        if (rawFFTs.size() > waterfallHeight + 300) {
            rawFFTs.resize(waterfallHeight);
        }

        memmove(&waterfallFb[dataWidth], waterfallFb, dataWidth * (waterfallHeight - 1) * sizeof(uint32_t));
        float pixel;
        float dataRange = waterfallMax - waterfallMin;
        for (int j = 0; j < dataWidth; j++) {
            pixel = (std::clamp<float>(latestFFT[j], waterfallMin, waterfallMax) - waterfallMin) / dataRange;
            int id = (int)(pixel * (WATERFALL_RESOLUTION - 1));
            waterfallFb[j] = waterfallPallet[id];
        }
        waterfallUpdate = true;
        buf_mtx.unlock();
    }

    void WaterFall::updatePallette(float colors[][3], int colorCount) {
        for (int i = 0; i < WATERFALL_RESOLUTION; i++) {
            int lowerId = floorf(((float)i / (float)WATERFALL_RESOLUTION) * colorCount);
            int upperId = ceilf(((float)i / (float)WATERFALL_RESOLUTION) * colorCount);
            lowerId = std::clamp<int>(lowerId, 0, colorCount - 1);
            upperId = std::clamp<int>(upperId, 0, colorCount - 1);
            float ratio = (((float)i / (float)WATERFALL_RESOLUTION) * colorCount) - lowerId;
            float r = (colors[lowerId][0] * (1.0f - ratio)) + (colors[upperId][0] * (ratio));
            float g = (colors[lowerId][1] * (1.0f - ratio)) + (colors[upperId][1] * (ratio));
            float b = (colors[lowerId][2] * (1.0f - ratio)) + (colors[upperId][2] * (ratio));
            waterfallPallet[i] = ((uint32_t)255 << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
        }
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

    void WaterFall::setCenterFrequency(float freq) {
        centerFreq = freq;
        lowerFreq = (centerFreq + viewOffset) - (viewBandwidth / 2.0f);
        upperFreq = (centerFreq + viewOffset) + (viewBandwidth / 2.0f);
        updateAllVFOs();
    }

    float WaterFall::getCenterFrequency() {
        return centerFreq;
    }

    void WaterFall::setBandwidth(float bandWidth) {
        float currentRatio = viewBandwidth / wholeBandwidth;
        wholeBandwidth = bandWidth;
        setViewBandwidth(bandWidth * currentRatio);
        updateAllVFOs();
    }

    float WaterFall::getBandwidth() {
        return wholeBandwidth;
    }

    void WaterFall::setViewBandwidth(float bandWidth) {
        if (bandWidth == viewBandwidth) {
            return;
        }
        if (abs(viewOffset) + (bandWidth / 2.0f) > wholeBandwidth / 2.0f) {
            if (viewOffset < 0) {
                viewOffset = (bandWidth / 2.0f) - (wholeBandwidth / 2.0f);
            }
            else {
                viewOffset = (wholeBandwidth / 2.0f) - (bandWidth / 2.0f);
            }
        }
        viewBandwidth = bandWidth;
        lowerFreq = (centerFreq + viewOffset) - (viewBandwidth / 2.0f);
        upperFreq = (centerFreq + viewOffset) + (viewBandwidth / 2.0f);
        range = findBestRange(bandWidth, maxHSteps);
        updateWaterfallFb();
        updateAllVFOs();
    }

    float WaterFall::getViewBandwidth() {
        return viewBandwidth;
    }

    void WaterFall::setViewOffset(float offset) {
        if (offset == viewOffset) {
            return;
        }
        if (offset - (viewBandwidth / 2.0f) < -(wholeBandwidth / 2.0f)) {
            offset = (viewBandwidth / 2.0f) - (wholeBandwidth / 2.0f);
        }
        if (offset + (viewBandwidth / 2.0f) > (wholeBandwidth / 2.0f)) {
            offset = (wholeBandwidth / 2.0f) - (viewBandwidth / 2.0f);
        }
        viewOffset = offset;
        lowerFreq = (centerFreq + viewOffset) - (viewBandwidth / 2.0f);
        upperFreq = (centerFreq + viewOffset) + (viewBandwidth / 2.0f);
        updateWaterfallFb();
        updateAllVFOs();
    }

    float WaterFall::getViewOffset() {
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

    void WaterFall::setWaterfallMin(float min) {
        if (min == waterfallMin) {
            return;
        }
        waterfallMin = min;
        updateWaterfallFb();
    }

    float WaterFall::getWaterfallMin() {
        return waterfallMin;
    }

    void WaterFall::setWaterfallMax(float max) {
        if (max == waterfallMax) {
            return;
        }
        waterfallMax = max;
        updateWaterfallFb();
    }

    float WaterFall::getWaterfallMax() {
        return waterfallMax;
    }

    void WaterFall::updateAllVFOs() {
        for (auto const& [name, vfo] : vfos) {
            vfo->updateDrawingVars(viewBandwidth, dataWidth, viewOffset, widgetPos, fftHeight);
        }
    }

    void WaterfallVFO::setOffset(float offset) {
        generalOffset = offset;
        if (reference == REF_CENTER) {
            centerOffset = offset;
            lowerOffset = offset - (bandwidth / 2.0f);
            upperOffset = offset + (bandwidth / 2.0f);
        }
        else if (reference == REF_LOWER) {
            lowerOffset = offset;
            centerOffset = offset + (bandwidth / 2.0f);
            upperOffset = offset + bandwidth;
        }
        else if (reference == REF_UPPER) {
            upperOffset = offset;
            centerOffset = offset - (bandwidth / 2.0f);
            lowerOffset = offset - bandwidth;
        }
        centerOffsetChanged = true;
        upperOffsetChanged = true;
        lowerOffsetChanged = true;
        redrawRequired = true;
    }

    void WaterfallVFO::setCenterOffset(float offset) {
        if (reference == REF_CENTER) {
            generalOffset = offset;
        }
        else if (reference == REF_LOWER) {
            generalOffset = offset - (bandwidth / 2.0f);
        }
        else if (reference == REF_UPPER) {
            generalOffset = offset + (bandwidth / 2.0f);
        }
        centerOffset = offset;
        lowerOffset = offset - (bandwidth / 2.0f);
        upperOffset = offset + (bandwidth / 2.0f);
        centerOffsetChanged = true;
        upperOffsetChanged = true;
        lowerOffsetChanged = true;
        redrawRequired = true;
    }

    void WaterfallVFO::setBandwidth(float bw) {
        if (bandwidth == bw || bw < 0) {
            return;
        }
        bandwidth = bw;
        if (reference == REF_CENTER) {
            lowerOffset = centerOffset - (bandwidth / 2.0f);
            upperOffset = centerOffset + (bandwidth / 2.0f);
        }
        else if (reference == REF_LOWER) {
            centerOffset = lowerOffset + (bandwidth / 2.0f);
            upperOffset = lowerOffset + bandwidth;
            centerOffsetChanged;
        }
        else if (reference == REF_UPPER) {
            centerOffset = upperOffset - (bandwidth / 2.0f);
            lowerOffset = upperOffset - bandwidth;
            centerOffsetChanged;
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

    void WaterfallVFO::updateDrawingVars(float viewBandwidth, float dataWidth, float viewOffset, ImVec2 widgetPos, int fftHeight) {
        float width = (bandwidth / viewBandwidth) * (float)dataWidth;
        int center = roundf((((centerOffset - viewOffset) / (viewBandwidth / 2.0f)) + 1.0f) * ((float)dataWidth / 2.0f));
        int left = roundf((((lowerOffset - viewOffset) / (viewBandwidth / 2.0f)) + 1.0f) * ((float)dataWidth / 2.0f));
        int right = roundf((((upperOffset - viewOffset) / (viewBandwidth / 2.0f)) + 1.0f) * ((float)dataWidth / 2.0f));

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
};

