#include <waterfall.h>
#include <algorithm>


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
    float factor = (float)width / (float)outWidth;
    for (int i = 0; i < outWidth; i++) {
        out[i] = data[offset + ((float)i * factor)];
    }
}

float freq_ranges[] = {
        1000.0f, 2000.0f, 2500.0f, 5000.0f,
        10000.0f, 20000.0f, 25000.0f, 50000.0f,
        100000.0f, 200000.0f, 250000.0f, 500000.0f,
        1000000.0f, 2000000.0f, 2500000.0f, 5000000.0f,
        10000000.0f, 20000000.0f, 25000000.0f, 50000000.0f
};

float findBestFreqRange(float bandwidth) {
    for (int i = 0; i < 15; i++) {
        if (bandwidth / freq_ranges[i] < 15.0f) {
            return freq_ranges[i];
        }
    }
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
        float startLine = floorf(fftMax / 10.0f) * 10.0f;
        float vertRange = fftMax - fftMin;
        float scaleFactor = fftHeight / vertRange;
        char buf[100];
        

        // Vertical scale
        for (float line = startLine; line > fftMin; line -= 10.0f) {
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
                                    ImVec2(widgetPos.x + 50 + i, roundf(bPos)),
                                    IM_COL32(0, 255, 255, 255), 1.0f);
            window->DrawList->AddLine(ImVec2(widgetPos.x + 50 + i, roundf(bPos)), 
                                    ImVec2(widgetPos.x + 50 + i, widgetPos.y + fftHeight + 10),
                                    IM_COL32(0, 255, 255, 50), 1.0f);
        }

        // X Axis
        window->DrawList->AddLine(ImVec2(widgetPos.x + 50, widgetPos.y + fftHeight + 10), 
                                    ImVec2(widgetPos.x + dataWidth + 50, widgetPos.y + fftHeight + 10),
                                    IM_COL32(255, 255, 255, 255), 1.0f);
        // Y Axis
        window->DrawList->AddLine(ImVec2(widgetPos.x + 50, widgetPos.y + 10), 
                                    ImVec2(widgetPos.x + 50, widgetPos.y + fftHeight + 10),
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

    void WaterFall::updateWaterfallFb() {
        float offsetRatio = viewOffset / (wholeBandwidth / 2.0f);
        int drawDataSize;
        int drawDataStart;
        int count = std::min<int>(waterfallHeight, rawFFTs.size());
        float* tempData = new float[dataWidth];
        float pixel;
        float dataRange = waterfallMax - waterfallMin;
        for (int i = 0; i < count; i++) {
            drawDataSize = (viewBandwidth / wholeBandwidth) * rawFFTs[i].size();
            drawDataStart = (((float)rawFFTs[i].size() / 2.0f) * (offsetRatio + 1)) - (drawDataSize / 2);
            doZoom(drawDataStart, drawDataSize, dataWidth, rawFFTs[i], tempData);
            for (int j = 0; j < dataWidth; j++) {
                pixel = (std::clamp<float>(tempData[j], waterfallMin, waterfallMax) - waterfallMin) / dataRange;
                waterfallFb[(i * dataWidth) + j] = waterfallPallet[(int)(pixel * (WATERFALL_RESOLUTION - 1))];
            }
        }
        delete[] tempData;
        waterfallUpdate = true;
    }

    void WaterFall::updateWaterfallTexture() {
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, dataWidth, waterfallHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, (uint8_t*)waterfallFb);
    }

    void WaterFall::onPositionChange() {
        printf("Pos changed\n");
    }

    void WaterFall::onResize() {
        printf("Resized\n");
        dataWidth = widgetSize.x - 60.0f;
        waterfallHeight = widgetSize.y - fftHeight - 52;
        delete[] latestFFT;
        delete[] waterfallFb;
        latestFFT = new float[dataWidth];
        waterfallFb = new uint32_t[dataWidth * waterfallHeight];
        for (int i = 0; i < dataWidth; i++) {
            latestFFT[i] = -1000.0f; // Hide everything
        }
        updateWaterfallFb();
    }

    void WaterFall::draw() {
        buf_mtx.lock();
        window = GetCurrentWindow();
        widgetPos = ImGui::GetWindowContentRegionMin();
        widgetEndPos = ImGui::GetWindowContentRegionMax();
        widgetPos.x += window->Pos.x;
        widgetPos.y += window->Pos.y;
        widgetEndPos.x += window->Pos.x;
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

        window->DrawList->AddRect(widgetPos, widgetEndPos, IM_COL32( 50, 50, 50, 255 ));
        window->DrawList->AddLine(ImVec2(widgetPos.x, widgetPos.y + fftHeight + 50), ImVec2(widgetPos.x + widgetSize.x, widgetPos.y + fftHeight + 50), IM_COL32(50, 50, 50, 255), 1.0f);

        drawFFT();
        drawWaterfall();
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

        memcpy(&waterfallFb[dataWidth], waterfallFb, dataWidth * (waterfallHeight - 1) * sizeof(uint32_t));
        float pixel;
        float dataRange = waterfallMax - waterfallMin;
        for (int j = 0; j < dataWidth; j++) {
            pixel = (std::clamp<float>(latestFFT[j], waterfallMin, waterfallMax) - waterfallMin) / dataRange;
            int id = (int)(pixel * (WATERFALL_RESOLUTION - 1));
            waterfallFb[j] = waterfallPallet[(int)(pixel * (WATERFALL_RESOLUTION - 1))];
        }
        waterfallUpdate = true;
        buf_mtx.unlock();
    }

    void WaterFall::updatePallette(float colors[][3], int colorCount) {
        for (int i = 0; i < WATERFALL_RESOLUTION; i++) {
            int lowerId = floorf(((float)i / (float)WATERFALL_RESOLUTION) * colorCount);
            int upperId = ceilf(((float)i / (float)WATERFALL_RESOLUTION) * colorCount);
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
    }

    float WaterFall::getCenterFrequency() {
        return centerFreq;
    }

    void WaterFall::setBandwidth(float bandWidth) {
        float currentRatio = viewBandwidth / wholeBandwidth;
        wholeBandwidth = bandWidth;
        setViewBandwidth(bandWidth * currentRatio);
    }

    float WaterFall::getBandwidth() {
        return wholeBandwidth;
    }

    void WaterFall::setVFOOffset(float offset) {
        vfoOffset = offset;
    }

    float WaterFall::getVFOOfset() {
        return vfoOffset;
    }

    void WaterFall::setVFOBandwidth(float bandwidth) {
        vfoBandwidth = bandwidth;
    }

    float WaterFall::getVFOBandwidth() {
        return vfoBandwidth;
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
        range = findBestFreqRange(bandWidth);
        updateWaterfallFb();
    }

    void WaterFall::setViewOffset(float offset) {
        if (offset == viewOffset) {
            return;
        }
        if (abs(offset) + (viewBandwidth / 2.0f) > (wholeBandwidth / 2.0f)) {
            return;
        }
        viewOffset = offset;
        lowerFreq = (centerFreq + viewOffset) - (viewBandwidth / 2.0f);
        upperFreq = (centerFreq + viewOffset) + (viewBandwidth / 2.0f);
        updateWaterfallFb();
    }
    
    void WaterFall::setFFTMin(float min) {
        fftMin = min;
    }

    float WaterFall::getFFTMin() {
        return fftMin;
    }

    void WaterFall::setFFTMax(float max) {
        fftMax = max;
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
};

