#include <waterfall.h>
#include <algorithm>

#define MAP_VAL(aMin, aMax, bMin, bMax, val) ( ( (   ((val) - (aMin)) / ((aMax) - (aMin))  )  * ((bMax) - (bMin)) )  + bMin)

const float COLOR_MAP[][3] = {
    {0x4A, 0x00, 0x00},
    {0x75, 0x00, 0x00},
    {0x9F, 0x00, 0x00},
    {0xC6, 0x00, 0x00},
    {0xFF, 0x00, 0x00},
    {0xFE, 0x6D, 0x16},
    {0xFF, 0xFF, 0x00},
    {0xFF, 0xFF, 0xFF},
    {0x1E, 0x90, 0xFF},
    {0x00, 0x00, 0x91},
    {0x00, 0x00, 0x50},
    {0x00, 0x00, 0x30},
    {0x00, 0x00, 0x20}
};

namespace ImGui {
    WaterFall::WaterFall() {
        std::vector<float> base;
        for (int i = 0; i < 1024; i++) {
            base.push_back(-100.0f);
        }
        fftBuffer.push_back(base);
        newSamples = false;
        glGenTextures(1, &textureId);
    }

    void WaterFall::drawFFT(ImGuiWindow* window, int width, int height, ImVec2 pos) {
        float lineHeight = (float)(height - 20 - 30) / 7.0f;
        char buf[100];
        int fftWidth = width - 50;

        // Vertical scale
        for (int i = 0; i < 8; i++) {
            sprintf(buf, "%d", -i * 10);
            ImVec2 txtSz = ImGui::CalcTextSize(buf);
            window->DrawList->AddText(ImVec2(pos.x + 30 - txtSz.x, pos.y + (i * lineHeight) + 2), IM_COL32( 255, 255, 255, 255 ), buf);
            if (i == 7) {
                window->DrawList->AddLine(ImVec2(pos.x + 40, pos.y + (i * lineHeight) + 10),
                                        ImVec2(pos.x + width - 10, pos.y + (i * lineHeight) + 10),
                                        IM_COL32( 255, 255, 255, 255 ), 1.0f);
                break;
            }
            window->DrawList->AddLine(ImVec2(pos.x + 40, pos.y + (i * lineHeight) + 10), 
                                    ImVec2(pos.x + width - 10, pos.y + (i * lineHeight) + 10), 
                                    IM_COL32( 70, 70, 70, 255 ), 1.0f);
        }

        // Horizontal scale
        float start = ceilf((centerFrequency - (bandWidth / 2)) / range) * range;
        float end = centerFrequency + (bandWidth / 2);
        float offsetStart = start - (centerFrequency - (bandWidth / 2));
        float pixelOffset = (offsetStart * fftWidth) / bandWidth;
        float pixelWidth = (range * fftWidth) / bandWidth;
        int count = 0;
        for (; start < end; start += range) {
            window->DrawList->AddLine(ImVec2(pos.x + pixelOffset + (pixelWidth * count) + 40, pos.y + 10),
                                    ImVec2(pos.x + pixelOffset + (pixelWidth * count) + 40, pos.y + (7 * lineHeight) + 10), 
                                    IM_COL32( 70, 70, 70, 255 ), 1.0f);

            window->DrawList->AddLine(ImVec2(pos.x + pixelOffset + (pixelWidth * count) + 40, pos.y + (7 * lineHeight) + 10),
                                    ImVec2(pos.x + pixelOffset + (pixelWidth * count) + 40, pos.y + (7 * lineHeight) + 20), 
                                    IM_COL32( 255, 255, 255, 255 ), 1.0f);

            sprintf(buf, "%.1fM", start / 1000000.0f);

            ImVec2 txtSz = ImGui::CalcTextSize(buf);

            window->DrawList->AddText(ImVec2(pos.x + pixelOffset + (pixelWidth * count) + 40 - (txtSz.x / 2.0f), pos.y + (7 * lineHeight) + 25), IM_COL32( 255, 255, 255, 255 ), buf);

            count++;
        }

        int dataCount = fftBuffer[0].size();
        float multiplier = (float)dataCount / (float)fftWidth;
        for (int i = 1; i < fftWidth; i++) {
            float a = (fftBuffer[0][(int)((float)(i - 1) * multiplier)] / 10.0f) * lineHeight;
            float b = (fftBuffer[0][(int)((float)i * multiplier)] / 10.0f) * lineHeight;
            window->DrawList->AddLine(ImVec2(pos.x + i + 39, pos.y - a), 
                                    ImVec2(pos.x + i + 40, pos.y - b), 
                                    IM_COL32( 0, 255, 255, 255 ), 1.0f);

            window->DrawList->AddLine(ImVec2(pos.x + i + 39, pos.y - a), 
                                    ImVec2(pos.x + i + 39, pos.y + (7.0f * lineHeight) + 9), 
                                    IM_COL32( 0, 255, 255, 50 ), 1.0f);
        }

        // window->DrawList->AddLine(ImVec2(pos.x + ((i - 1) * spacing) + 40, pos.y - a), 
        //                          ImVec2(pos.x + (i * spacing) + 40, pos.y - b), 
        //                          IM_COL32( 0, 255, 255, 255 ), 1.0f);

        window->DrawList->AddLine(ImVec2(pos.x + 40, pos.y + 10), ImVec2(pos.x + 40, pos.y + (7 * lineHeight) + 10), IM_COL32( 255, 255, 255, 255 ), 1.0f);

        ImVec2 mPos = ImGui::GetMousePos();
        window->DrawList->AddRectFilled(ImVec2(mPos.x - 20, pos.y + 1), ImVec2(mPos.x + 20, pos.y + (7 * lineHeight) + 10), IM_COL32( 255, 255, 255, 50 ));
        window->DrawList->AddLine(ImVec2(mPos.x, pos.y + 1), ImVec2(mPos.x, pos.y + (7 * lineHeight) + 10), IM_COL32( 255, 0, 0, 255 ), 1.0f);

        
    }

    uint32_t mapColor(float val) {
        float mapped = MAP_VAL(-50.0f, 0.0f, 0, 12, val);
        mapped = std::max<float>(mapped, 0.0f);
        mapped = std::min<float>(mapped, 12.0f);
        int floored = floorf(mapped);
        float ratio = mapped - (float)floored;

        float r = ((COLOR_MAP[floored][2] * (1.0f - ratio)) + (COLOR_MAP[floored + 1][2] * ratio));
        float g = ((COLOR_MAP[floored][1] * (1.0f - ratio)) + (COLOR_MAP[floored + 1][1] * ratio));
        float b = ((COLOR_MAP[floored][0] * (1.0f - ratio)) + (COLOR_MAP[floored + 1][0] * ratio));

        //printf("%f %f %f\n", r, g, b);

        return ((uint32_t)255 << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
    }

    uint32_t* img = NULL;
    int lastW = 0;
    int lastH = 0;

    void WaterFall::drawWaterfall(ImGuiWindow* window, int width, int height, ImVec2 pos) {
        int w = width - 10;
        int h = height;
        int count = fftBuffer.size();
        float factor = (float)count / (float)w;
        bool newSize = false;

        if (lastW != w || lastH != h) {
            newSize = true;
            lastW = w;
            lastH = h;
            if (img != NULL) {
                free(img);
            }
            printf("Allocating new buffer");
            img = (uint32_t*)malloc(w * h * sizeof(uint32_t));
            newSamples = true;
        }

        if (newSamples || newSize) {
            newSamples = false;
            float factor;

            if (newSize) {
                for (int y = 0; y < count; y++) {
                    factor = (float)fftBuffer[y].size() / (float)w;
                    for (int x = 0; x < w; x++) {
                        img[(y * w) + x] = mapColor(fftBuffer[y][(int)((float)x * factor)]);
                    }
                }
            }
            else {
                factor = (float)fftBuffer[0].size() / (float)w;
                memcpy(&img[w], img, (h - 1) * w * sizeof(uint32_t));
                for (int x = 0; x < w; x++) {
                    img[x] = mapColor(fftBuffer[0][(int)((float)x * factor)]);
                }
            }
            

            glBindTexture(GL_TEXTURE_2D, textureId);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
        }

        

        window->DrawList->AddImage((void*)(intptr_t)textureId, ImVec2(pos.x + 40, pos.y), ImVec2(pos.x + w, pos.y + h));
    }

    void WaterFall::draw() {
        ImGuiWindow* window = GetCurrentWindow();
        ImVec2 vMin = ImGui::GetWindowContentRegionMin();
        ImVec2 vMax = ImGui::GetWindowContentRegionMax();
        vMin.x += ImGui::GetWindowPos().x;
        vMin.y += ImGui::GetWindowPos().y;
        vMax.x += ImGui::GetWindowPos().x;
        vMax.y += ImGui::GetWindowPos().y;
        int width = vMax.x - vMin.x;
        int height = vMax.y - vMin.y;
        window->DrawList->AddRect( vMin, vMax, IM_COL32( 50, 50, 50, 255 ) );

        window->DrawList->AddLine(ImVec2(vMin.x, vMin.y + 300), ImVec2(vMin.x + width, vMin.y + 300), IM_COL32( 50, 50, 50, 255 ), 1.0f);

        buf_mtx.lock();
        if (fftBuffer.size() > height - 302) {
            fftBuffer.resize(height - 302);
        }
        drawFFT(window, width, 300, vMin);
        drawWaterfall(window, width - 2, height - 302, ImVec2(vMin.x + 1, vMin.y + 301));
        buf_mtx.unlock();
    }

    void WaterFall::pushFFT(std::vector<float> data, int n) {
        buf_mtx.lock();
        fftBuffer.insert(fftBuffer.begin(), data);
        newSamples = true;
        buf_mtx.unlock();
    }
};

