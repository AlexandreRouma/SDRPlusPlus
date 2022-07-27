#include <gui/widgets/waterfall.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imutils.h>
#include <algorithm>
#include <volk/volk.h>
#include <spdlog/spdlog.h>
#include <gui/gui.h>
#include <gui/style.h>

float DEFAULT_COLOR_MAP[][3] = {
    { 0x00, 0x00, 0x20 },
    { 0x00, 0x00, 0x30 },
    { 0x00, 0x00, 0x50 },
    { 0x00, 0x00, 0x91 },
    { 0x1E, 0x90, 0xFF },
    { 0xFF, 0xFF, 0xFF },
    { 0xFF, 0xFF, 0x00 },
    { 0xFE, 0x6D, 0x16 },
    { 0xFF, 0x00, 0x00 },
    { 0xC6, 0x00, 0x00 },
    { 0x9F, 0x00, 0x00 },
    { 0x75, 0x00, 0x00 },
    { 0x4A, 0x00, 0x00 }
};

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

inline double findBestRange(double bandwidth, int maxSteps) {
    for (int i = 0; i < 32; i++) {
        if (bandwidth / freq_ranges[i] < (double)maxSteps) {
            return freq_ranges[i];
        }
    }
    return 50000000.0;
}

inline void printAndScale(double freq, char* buf) {
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
        latestFFT = new float[dataWidth];
        latestFFTHold = new float[dataWidth];
        waterfallFb = new uint32_t[1];

        viewBandwidth = 1.0;
        wholeBandwidth = 1.0;

        updatePallette(DEFAULT_COLOR_MAP, 13);
    }

    void WaterFall::init() {
        glGenTextures(1, &textureId);
    }

    void WaterFall::drawFFT() {
        std::lock_guard<std::recursive_mutex> lck(latestFFTMtx);
        // Calculate scaling factor
        float startLine = floorf(fftMax / vRange) * vRange;
        float vertRange = fftMax - fftMin;
        float scaleFactor = fftHeight / vertRange;
        char buf[100];

        ImU32 trace = ImGui::GetColorU32(ImGuiCol_PlotLines);
        ImU32 traceHold = ImGui::ColorConvertFloat4ToU32(gui::themeManager.fftHoldColor);
        ImU32 shadow = ImGui::GetColorU32(ImGuiCol_PlotLines, 0.2);
        ImU32 text = ImGui::GetColorU32(ImGuiCol_Text);
        float textVOffset = 10.0f * style::uiScale;

        // Vertical scale
        for (float line = startLine; line > fftMin; line -= vRange) {
            float yPos = fftAreaMax.y - ((line - fftMin) * scaleFactor);
            window->DrawList->AddLine(ImVec2(fftAreaMin.x, roundf(yPos)),
                                      ImVec2(fftAreaMax.x, roundf(yPos)),
                                      IM_COL32(50, 50, 50, 255), style::uiScale);
            sprintf(buf, "%d", (int)line);
            ImVec2 txtSz = ImGui::CalcTextSize(buf);
            window->DrawList->AddText(ImVec2(fftAreaMin.x - txtSz.x - textVOffset, roundf(yPos - (txtSz.y / 2.0))), text, buf);
        }

        // Horizontal scale
        double startFreq = ceilf(lowerFreq / range) * range;
        double horizScale = (double)dataWidth / viewBandwidth;
        float scaleVOfsset = 7 * style::uiScale;
        for (double freq = startFreq; freq < upperFreq; freq += range) {
            double xPos = fftAreaMin.x + ((freq - lowerFreq) * horizScale);
            window->DrawList->AddLine(ImVec2(roundf(xPos), fftAreaMin.y + 1),
                                      ImVec2(roundf(xPos), fftAreaMax.y),
                                      IM_COL32(50, 50, 50, 255), style::uiScale);
            window->DrawList->AddLine(ImVec2(roundf(xPos), fftAreaMax.y),
                                      ImVec2(roundf(xPos), fftAreaMax.y + scaleVOfsset),
                                      text, style::uiScale);
            printAndScale(freq, buf);
            ImVec2 txtSz = ImGui::CalcTextSize(buf);
            window->DrawList->AddText(ImVec2(roundf(xPos - (txtSz.x / 2.0)), fftAreaMax.y + txtSz.y), text, buf);
        }

        // Data
        if (latestFFT != NULL && fftLines != 0) {
            for (int i = 1; i < dataWidth; i++) {
                double aPos = fftAreaMax.y - ((latestFFT[i - 1] - fftMin) * scaleFactor);
                double bPos = fftAreaMax.y - ((latestFFT[i] - fftMin) * scaleFactor);
                aPos = std::clamp<double>(aPos, fftAreaMin.y + 1, fftAreaMax.y);
                bPos = std::clamp<double>(bPos, fftAreaMin.y + 1, fftAreaMax.y);
                window->DrawList->AddLine(ImVec2(fftAreaMin.x + i - 1, roundf(aPos)),
                                          ImVec2(fftAreaMin.x + i, roundf(bPos)), trace, 1.0);
                window->DrawList->AddLine(ImVec2(fftAreaMin.x + i, roundf(bPos)),
                                          ImVec2(fftAreaMin.x + i, fftAreaMax.y), shadow, 1.0);
            }
        }

        // Hold
        if (fftHold && latestFFT != NULL && latestFFTHold != NULL && fftLines != 0) {
            for (int i = 1; i < dataWidth; i++) {
                double aPos = fftAreaMax.y - ((latestFFTHold[i - 1] - fftMin) * scaleFactor);
                double bPos = fftAreaMax.y - ((latestFFTHold[i] - fftMin) * scaleFactor);
                aPos = std::clamp<double>(aPos, fftAreaMin.y + 1, fftAreaMax.y);
                bPos = std::clamp<double>(bPos, fftAreaMin.y + 1, fftAreaMax.y);
                window->DrawList->AddLine(ImVec2(fftAreaMin.x + i - 1, roundf(aPos)),
                                          ImVec2(fftAreaMin.x + i, roundf(bPos)), traceHold, 1.0);
            }
        }

        FFTRedrawArgs args;
        args.min = fftAreaMin;
        args.max = fftAreaMax;
        args.lowFreq = lowerFreq;
        args.highFreq = upperFreq;
        args.freqToPixelRatio = horizScale;
        args.pixelToFreqRatio = viewBandwidth / (double)dataWidth;
        args.window = window;
        onFFTRedraw.emit(args);

        // X Axis
        window->DrawList->AddLine(ImVec2(fftAreaMin.x, fftAreaMax.y),
                                  ImVec2(fftAreaMax.x, fftAreaMax.y),
                                  text, style::uiScale);
        // Y Axis
        window->DrawList->AddLine(ImVec2(fftAreaMin.x, fftAreaMin.y),
                                  ImVec2(fftAreaMin.x, fftAreaMax.y - 1),
                                  text, style::uiScale);
    }

    void WaterFall::drawWaterfall() {
        if (waterfallUpdate) {
            waterfallUpdate = false;
            updateWaterfallTexture();
        }
        {
            std::lock_guard<std::mutex> lck(texMtx);
            window->DrawList->AddImage((void*)(intptr_t)textureId, wfMin, wfMax);
        }
        
        ImVec2 mPos = ImGui::GetMousePos();

        if (IS_IN_AREA(mPos, wfMin, wfMax) && !gui::mainWindow.lockWaterfallControls && !inputHandled) {
            for (auto const& [name, vfo] : vfos) {
                window->DrawList->AddRectFilled(vfo->wfRectMin, vfo->wfRectMax, vfo->color);
                if (!vfo->lineVisible) { continue; }
                window->DrawList->AddLine(vfo->wfLineMin, vfo->wfLineMax, (name == selectedVFO) ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 255, 0, 255), style::uiScale);
            }
        }
    }

    void WaterFall::drawVFOs() {
        for (auto const& [name, vfo] : vfos) {
            vfo->draw(window, name == selectedVFO);
        }
    }

    void WaterFall::selectFirstVFO() {
        bool available = false;
        for (auto const& [name, vfo] : vfos) {
            available = true;
            selectedVFO = name;
            selectedVFOChanged = true;
            return;
        }
        if (!available) {
            selectedVFO = "";
            selectedVFOChanged = true;
        }
    }

    void WaterFall::processInputs() {
        // Pre calculate useful values
        WaterfallVFO* selVfo = NULL;
        if (selectedVFO != "") {
            selVfo = vfos[selectedVFO];
        }
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
        ImVec2 dragOrigin(mousePos.x - drag.x, mousePos.y - drag.y);

        bool mouseHovered, mouseHeld;
        bool mouseClicked = ImGui::ButtonBehavior(ImRect(fftAreaMin, wfMax), GetID("WaterfallID"), &mouseHovered, &mouseHeld,
                                                  ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_PressedOnClick);

        mouseInFFTResize = (dragOrigin.x > widgetPos.x && dragOrigin.x < widgetPos.x + widgetSize.x && dragOrigin.y >= widgetPos.y + newFFTAreaHeight - (2.0f * style::uiScale) && dragOrigin.y <= widgetPos.y + newFFTAreaHeight + (2.0f * style::uiScale));
        mouseInFreq = IS_IN_AREA(dragOrigin, freqAreaMin, freqAreaMax);
        mouseInFFT = IS_IN_AREA(dragOrigin, fftAreaMin, fftAreaMax);
        mouseInWaterfall = IS_IN_AREA(dragOrigin, wfMin, wfMax);

        int mouseWheel = ImGui::GetIO().MouseWheel;

        bool mouseMoved = false;
        if (mousePos.x != lastMousePos.x || mousePos.y != lastMousePos.y) { mouseMoved = true; }
        lastMousePos = mousePos;

        std::string hoveredVFOName = "";
        for (auto const& [name, _vfo] : vfos) {
            if (ImGui::IsMouseHoveringRect(_vfo->rectMin, _vfo->rectMax) || ImGui::IsMouseHoveringRect(_vfo->wfRectMin, _vfo->wfRectMax)) {
                hoveredVFOName = name;
                break;
            }
        }

        // Deselect everything if the mouse is released
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (fftResizeSelect) {
                FFTAreaHeight = newFFTAreaHeight;
                onResize();
            }

            fftResizeSelect = false;
            freqScaleSelect = false;
            vfoSelect = false;
            vfoBorderSelect = false;
            lastDrag = 0;
        }

        bool targetFound = false;

        // If the mouse was clicked anywhere in the waterfall, check if the resize was clicked
        if (mouseInFFTResize) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                fftResizeSelect = true;
                targetFound = true;
            }
        }

        // If mouse was clicked inside the central part, check what was clicked
        if (mouseClicked && !targetFound) {
            mouseDownPos = mousePos;

            // First, check if a VFO border was selected
            for (auto const& [name, _vfo] : vfos) {
                if (_vfo->bandwidthLocked) { continue; }
                if (_vfo->rectMax.x - _vfo->rectMin.x < 10) { continue; }
                bool resizing = false;
                if (_vfo->reference != REF_LOWER) {
                    if (IS_IN_AREA(mousePos, _vfo->lbwSelMin, _vfo->lbwSelMax)) { resizing = true; }
                    else if (IS_IN_AREA(mousePos, _vfo->wfLbwSelMin, _vfo->wfLbwSelMax)) {
                        resizing = true;
                    }
                }
                if (_vfo->reference != REF_UPPER) {
                    if (IS_IN_AREA(mousePos, _vfo->rbwSelMin, _vfo->rbwSelMax)) { resizing = true; }
                    else if (IS_IN_AREA(mousePos, _vfo->wfRbwSelMin, _vfo->wfRbwSelMax)) {
                        resizing = true;
                    }
                }
                if (!resizing) { continue; }
                relatedVfo = _vfo;
                vfoBorderSelect = true;
                targetFound = true;
                break;
            }

            // Next, check if a VFO was selected
            if (!targetFound && hoveredVFOName != "") {
                selectedVFO = hoveredVFOName;
                selectedVFOChanged = true;
                targetFound = true;
                return;
            }

            // Now, check frequency scale
            if (!targetFound && mouseInFreq) {
                freqScaleSelect = true;
            }
        }

        // If the FFT resize bar was selected, resize FFT accordingly
        if (fftResizeSelect) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
            newFFTAreaHeight = mousePos.y - widgetPos.y;
            newFFTAreaHeight = std::clamp<float>(newFFTAreaHeight, 150, widgetSize.y - 50);
            ImGui::GetForegroundDrawList()->AddLine(ImVec2(widgetPos.x, newFFTAreaHeight + widgetPos.y), ImVec2(widgetEndPos.x, newFFTAreaHeight + widgetPos.y),
                                                    ImGui::GetColorU32(ImGuiCol_SeparatorActive), style::uiScale);
            return;
        }

        // If a vfo border is selected, resize VFO accordingly
        if (vfoBorderSelect) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            double dist = (relatedVfo->reference == REF_CENTER) ? fabsf(mousePos.x - relatedVfo->lineMin.x) : (mousePos.x - relatedVfo->lineMin.x);
            if (relatedVfo->reference == REF_UPPER) { dist = -dist; }
            double hzDist = dist * (viewBandwidth / (double)dataWidth);
            if (relatedVfo->reference == REF_CENTER) {
                hzDist *= 2.0;
            }
            hzDist = std::clamp<double>(hzDist, relatedVfo->minBandwidth, relatedVfo->maxBandwidth);
            relatedVfo->setBandwidth(hzDist);
            relatedVfo->onUserChangedBandwidth.emit(hzDist);
            return;
        }

        // If the frequency scale is selected, move it
        if (freqScaleSelect) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            double deltax = drag.x - lastDrag;
            lastDrag = drag.x;
            double viewDelta = deltax * (viewBandwidth / (double)dataWidth);

            viewOffset -= viewDelta;

            if (viewOffset + (viewBandwidth / 2.0) > wholeBandwidth / 2.0) {
                double freqOffset = (viewOffset + (viewBandwidth / 2.0)) - (wholeBandwidth / 2.0);
                viewOffset = (wholeBandwidth / 2.0) - (viewBandwidth / 2.0);
                if (!centerFrequencyLocked) {
                    centerFreq += freqOffset;
                    centerFreqMoved = true;
                }
            }
            if (viewOffset - (viewBandwidth / 2.0) < -(wholeBandwidth / 2.0)) {
                double freqOffset = (viewOffset - (viewBandwidth / 2.0)) + (wholeBandwidth / 2.0);
                viewOffset = (viewBandwidth / 2.0) - (wholeBandwidth / 2.0);
                if (!centerFrequencyLocked) {
                    centerFreq += freqOffset;
                    centerFreqMoved = true;
                }
            }

            lowerFreq = (centerFreq + viewOffset) - (viewBandwidth / 2.0);
            upperFreq = (centerFreq + viewOffset) + (viewBandwidth / 2.0);

            if (viewBandwidth != wholeBandwidth) {
                updateAllVFOs();
                if (_fullUpdate) { updateWaterfallFb(); };
            }
            return;
        }

        // If the mouse wheel is moved on the frequency scale
        if (mouseWheel != 0 && mouseInFreq) {
            viewOffset -= (double)mouseWheel * viewBandwidth / 20.0;

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
            return;
        }

        // If the left and right keys are pressed while hovering the freq scale, move it too
        bool leftKeyPressed = ImGui::IsKeyPressed(ImGuiKey_LeftArrow);
        if ((leftKeyPressed || ImGui::IsKeyPressed(ImGuiKey_RightArrow)) && mouseInFreq) {
            viewOffset += leftKeyPressed ? (viewBandwidth / 20.0) : (-viewBandwidth / 20.0);

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
            return;
        }

        // Finally, if nothing else was selected, just move the VFO
        if ((VFOMoveSingleClick ? ImGui::IsMouseClicked(ImGuiMouseButton_Left) : ImGui::IsMouseDown(ImGuiMouseButton_Left)) && (mouseInFFT | mouseInWaterfall) && (mouseMoved || hoveredVFOName == "")) {
            if (selVfo != NULL) {
                int refCenter = mousePos.x - fftAreaMin.x;
                if (refCenter >= 0 && refCenter < dataWidth) {
                    double off = ((((double)refCenter / ((double)dataWidth / 2.0)) - 1.0) * (viewBandwidth / 2.0)) + viewOffset;
                    off += centerFreq;
                    off = (round(off / selVfo->snapInterval) * selVfo->snapInterval) - centerFreq;
                    selVfo->setOffset(off);
                }
            }
        }
        else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            // Check if a VFO is hovered. If yes, show tooltip
            for (auto const& [name, _vfo] : vfos) {
                if (ImGui::IsMouseHoveringRect(_vfo->rectMin, _vfo->rectMax) || ImGui::IsMouseHoveringRect(_vfo->wfRectMin, _vfo->wfRectMax)) {
                    char buf[128];
                    ImGui::BeginTooltip();

                    ImGui::TextUnformatted(name.c_str());

                    if (ImGui::GetIO().KeyCtrl) {
                        ImGui::Separator();
                        printAndScale(_vfo->generalOffset + centerFreq, buf);
                        ImGui::Text("Frequency: %sHz", buf);
                        printAndScale(_vfo->bandwidth, buf);
                        ImGui::Text("Bandwidth: %sHz", buf);
                        ImGui::Text("Bandwidth Locked: %s", _vfo->bandwidthLocked ? "Yes" : "No");

                        float strength, snr;
                        if (calculateVFOSignalInfo(waterfallVisible ? &rawFFTs[currentFFTLine * rawFFTSize] : rawFFTs, _vfo, strength, snr)) {
                            ImGui::Text("Strength: %0.1fdBFS", strength);
                            ImGui::Text("SNR: %0.1fdB", snr);
                        }
                        else {
                            ImGui::TextUnformatted("Strength: ---.-dBFS");
                            ImGui::TextUnformatted("SNR: ---.-dB");
                        }
                    }

                    ImGui::EndTooltip();
                    break;
                }
            }
        }

        // Handle Page Up to cycle through VFOs
        if (ImGui::IsKeyPressed(ImGuiKey_PageUp) && selVfo != NULL) {
            std::string next = (--vfos.end())->first;
            std::string lowest = "";
            double lowestOffset = INFINITY;
            double firstVfoOffset = selVfo->generalOffset;
            double smallestDistance = INFINITY;
            bool found = false;
            for (auto& [_name, _vfo] : vfos) {
                if (_vfo->generalOffset > firstVfoOffset && (_vfo->generalOffset - firstVfoOffset) < smallestDistance) {
                    next = _name;
                    smallestDistance = (_vfo->generalOffset - firstVfoOffset);
                    found = true;
                }
                if (_vfo->generalOffset < lowestOffset) {
                    lowestOffset = _vfo->generalOffset;
                    lowest = _name;
                }
            }
            selectedVFO = found ? next : lowest;
            selectedVFOChanged = true;
        }

        // Handle Page Down to cycle through VFOs
        if (ImGui::IsKeyPressed(ImGuiKey_PageDown) && selVfo != NULL) {
            std::string next = (--vfos.end())->first;
            std::string highest = "";
            double highestOffset = -INFINITY;
            double firstVfoOffset = selVfo->generalOffset;
            double smallestDistance = INFINITY;
            bool found = false;
            for (auto& [_name, _vfo] : vfos) {
                if (_vfo->generalOffset < firstVfoOffset && (firstVfoOffset - _vfo->generalOffset) < smallestDistance) {
                    next = _name;
                    smallestDistance = (firstVfoOffset - _vfo->generalOffset);
                    found = true;
                }
                if (_vfo->generalOffset > highestOffset) {
                    highestOffset = _vfo->generalOffset;
                    highest = _name;
                }
            }
            selectedVFO = found ? next : highest;
            selectedVFOChanged = true;
        }
    }

    bool WaterFall::calculateVFOSignalInfo(float* fftLine, WaterfallVFO* _vfo, float& strength, float& snr) {
        if (fftLine == NULL || fftLines <= 0) { return false; }

        // Calculate FFT index data
        double vfoMinSizeFreq = _vfo->centerOffset - _vfo->bandwidth;
        double vfoMinFreq = _vfo->centerOffset - (_vfo->bandwidth / 2.0);
        double vfoMaxFreq = _vfo->centerOffset + (_vfo->bandwidth / 2.0);
        double vfoMaxSizeFreq = _vfo->centerOffset + _vfo->bandwidth;
        int vfoMinSideOffset = std::clamp<int>(((vfoMinSizeFreq / (wholeBandwidth / 2.0)) * (double)(rawFFTSize / 2)) + (rawFFTSize / 2), 0, rawFFTSize);
        int vfoMinOffset = std::clamp<int>(((vfoMinFreq / (wholeBandwidth / 2.0)) * (double)(rawFFTSize / 2)) + (rawFFTSize / 2), 0, rawFFTSize);
        int vfoMaxOffset = std::clamp<int>(((vfoMaxFreq / (wholeBandwidth / 2.0)) * (double)(rawFFTSize / 2)) + (rawFFTSize / 2), 0, rawFFTSize);
        int vfoMaxSideOffset = std::clamp<int>(((vfoMaxSizeFreq / (wholeBandwidth / 2.0)) * (double)(rawFFTSize / 2)) + (rawFFTSize / 2), 0, rawFFTSize);

        double avg = 0;
        float max = -INFINITY;
        int avgCount = 0;

        // Calculate Left average
        for (int i = vfoMinSideOffset; i < vfoMinOffset; i++) {
            avg += fftLine[i];
            avgCount++;
        }

        // Calculate Right average
        for (int i = vfoMaxOffset + 1; i < vfoMaxSideOffset; i++) {
            avg += fftLine[i];
            avgCount++;
        }

        avg /= (double)(avgCount);

        // Calculate max
        for (int i = vfoMinOffset; i <= vfoMaxOffset; i++) {
            if (fftLine[i] > max) { max = fftLine[i]; }
        }

        strength = max;
        snr = max - avg;

        return true;
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
        if (rawFFTs != NULL && fftLines >= 0) {
            for (int i = 0; i < count; i++) {
                drawDataSize = (viewBandwidth / wholeBandwidth) * rawFFTSize;
                drawDataStart = (((double)rawFFTSize / 2.0) * (offsetRatio + 1)) - (drawDataSize / 2);
                doZoom(drawDataStart, drawDataSize, dataWidth, &rawFFTs[((i + currentFFTLine) % waterfallHeight) * rawFFTSize], tempData);
                for (int j = 0; j < dataWidth; j++) {
                    pixel = (std::clamp<float>(tempData[j], waterfallMin, waterfallMax) - waterfallMin) / dataRange;
                    waterfallFb[(i * dataWidth) + j] = waterfallPallet[(int)(pixel * (WATERFALL_RESOLUTION - 1))];
                }
            }

            for (int i = count; i < waterfallHeight; i++) {
                for (int j = 0; j < dataWidth; j++) {
                    waterfallFb[(i * dataWidth) + j] = (uint32_t)255 << 24;
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
            bpBottom = fftAreaMax.y;
        }
        else {
            bpBottom = fftAreaMin.y + height + 1;
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
            aPos = fftAreaMin.x + ((start - lowerFreq) * horizScale);
            bPos = fftAreaMin.x + ((end - lowerFreq) * horizScale);
            cPos = fftAreaMin.x + ((center - lowerFreq) * horizScale);
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
            if (aPos <= fftAreaMin.x) {
                aPos = fftAreaMin.x + 1;
            }
            if (bPos <= fftAreaMin.x) {
                bPos = fftAreaMin.x + 1;
            }
            if (width >= 1.0) {
                window->DrawList->AddRectFilled(ImVec2(roundf(aPos), bpBottom - height),
                                                ImVec2(roundf(bPos), bpBottom), colorTrans);
                if (startVis) {
                    window->DrawList->AddLine(ImVec2(roundf(aPos), bpBottom - height - 1),
                                              ImVec2(roundf(aPos), bpBottom - 1), color, style::uiScale);
                }
                if (endVis) {
                    window->DrawList->AddLine(ImVec2(roundf(bPos), bpBottom - height - 1),
                                              ImVec2(roundf(bPos), bpBottom - 1), color, style::uiScale);
                }
            }
            if (txtSz.x <= width) {
                window->DrawList->AddText(ImVec2(cPos - (txtSz.x / 2.0), bpBottom - (height / 2.0f) - (txtSz.y / 2.0f)),
                                          IM_COL32(255, 255, 255, 255), bandplan->bands[i].name.c_str());
            }
        }
    }

    void WaterFall::updateWaterfallTexture() {
        std::lock_guard<std::mutex> lck(texMtx);
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
        std::lock_guard<std::recursive_mutex> lck(latestFFTMtx);
        // return if widget is too small
        if (widgetSize.x < 100 || widgetSize.y < 100) {
            return;
        }

        int lastWaterfallHeight = waterfallHeight;

        if (waterfallVisible) {
            FFTAreaHeight = std::min<int>(FFTAreaHeight, widgetSize.y - (50.0f * style::uiScale));
            newFFTAreaHeight = FFTAreaHeight;
            fftHeight = FFTAreaHeight - (50.0f * style::uiScale);
            waterfallHeight = widgetSize.y - fftHeight - (50.0f * style::uiScale) - 2;
        }
        else {
            fftHeight = widgetSize.y - (50.0f * style::uiScale);
        }
        dataWidth = widgetSize.x - (60.0f * style::uiScale);

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

        // Reallocate display FFT
        if (latestFFT != NULL) {
            delete[] latestFFT;
        }
        latestFFT = new float[dataWidth];

        // Reallocate hold FFT
        if (latestFFTHold != NULL) {
            delete[] latestFFTHold;
        }
        latestFFTHold = new float[dataWidth];

        if (waterfallVisible) {
            delete[] waterfallFb;
            waterfallFb = new uint32_t[dataWidth * waterfallHeight];
            memset(waterfallFb, 0, dataWidth * waterfallHeight * sizeof(uint32_t));
        }
        for (int i = 0; i < dataWidth; i++) {
            latestFFT[i] = -1000.0; // Hide everything
            latestFFTHold[i] = -1000.0;
        }

        fftAreaMin = ImVec2(widgetPos.x + (50.0f * style::uiScale), widgetPos.y + (9.0f * style::uiScale));
        fftAreaMax = ImVec2(fftAreaMin.x + dataWidth, fftAreaMin.y + fftHeight + 1);

        freqAreaMin = ImVec2(fftAreaMin.x, fftAreaMax.y + 1);
        freqAreaMax = ImVec2(fftAreaMax.x, fftAreaMax.y + (40.0f * style::uiScale));

        wfMin = ImVec2(fftAreaMin.x, freqAreaMax.y + 1);
        wfMax = ImVec2(fftAreaMin.x + dataWidth, wfMin.y + waterfallHeight);

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

        //window->DrawList->AddRectFilled(widgetPos, widgetEndPos, IM_COL32( 0, 0, 0, 255 ));
        ImU32 bg = ImGui::ColorConvertFloat4ToU32(gui::themeManager.waterfallBg);
        window->DrawList->AddRectFilled(widgetPos, widgetEndPos, bg);
        window->DrawList->AddRect(widgetPos, widgetEndPos, IM_COL32(50, 50, 50, 255), 0.0, 0, style::uiScale);
        window->DrawList->AddLine(ImVec2(widgetPos.x, freqAreaMax.y), ImVec2(widgetPos.x + widgetSize.x, freqAreaMax.y), IM_COL32(50, 50, 50, 255), style::uiScale);

        if (!gui::mainWindow.lockWaterfallControls) {
            inputHandled = false;
            InputHandlerArgs args;
            args.fftRectMin = fftAreaMin;
            args.fftRectMax = fftAreaMax;
            args.freqScaleRectMin = freqAreaMin;
            args.freqScaleRectMax = freqAreaMax;
            args.waterfallRectMin = wfMin;
            args.waterfallRectMax = wfMax;
            args.lowFreq = lowerFreq;
            args.highFreq = upperFreq;
            args.freqToPixelRatio = (double)dataWidth / viewBandwidth;
            args.pixelToFreqRatio = viewBandwidth / (double)dataWidth;
            onInputProcess.emit(args);
            if (!inputHandled) { processInputs(); }
        }

        updateAllVFOs(true);

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
        std::lock_guard<std::recursive_mutex> lck(latestFFTMtx);
        double offsetRatio = viewOffset / (wholeBandwidth / 2.0);
        int drawDataSize = (viewBandwidth / wholeBandwidth) * rawFFTSize;
        int drawDataStart = (((double)rawFFTSize / 2.0) * (offsetRatio + 1)) - (drawDataSize / 2);

        if (waterfallVisible) {
            doZoom(drawDataStart, drawDataSize, dataWidth, &rawFFTs[currentFFTLine * rawFFTSize], latestFFT);
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
            doZoom(drawDataStart, drawDataSize, dataWidth, rawFFTs, latestFFT);
            fftLines = 1;
        }

        if (selectedVFO != "" && vfos.size() > 0) {
            float dummy;
            calculateVFOSignalInfo(waterfallVisible ? &rawFFTs[currentFFTLine * rawFFTSize] : rawFFTs, vfos[selectedVFO], dummy, selectedVFOSNR);
        }

        // If FFT hold is enabled, update it
        if (fftHold && latestFFT != NULL && latestFFTHold != NULL && fftLines != 0) {
            for (int i = 1; i < dataWidth; i++) {
                latestFFTHold[i] = std::max<float>(latestFFT[i], latestFFTHold[i] - fftHoldSpeed);
            }
        }

        buf_mtx.unlock();
    }

    void WaterFall::updatePallette(float colors[][3], int colorCount) {
        std::lock_guard<std::recursive_mutex> lck(buf_mtx);
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
        std::lock_guard<std::recursive_mutex> lck(buf_mtx);
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
        std::lock_guard<std::recursive_mutex> lck(latestFFTMtx);
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
        std::lock_guard<std::recursive_mutex> lck(buf_mtx);
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
        std::lock_guard<std::recursive_mutex> lck(buf_mtx);
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
        std::lock_guard<std::recursive_mutex> lck(buf_mtx);
        _fullUpdate = fullUpdate;
    }

    void WaterFall::setWaterfallMin(float min) {
        std::lock_guard<std::recursive_mutex> lck(buf_mtx);
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
        std::lock_guard<std::recursive_mutex> lck(buf_mtx);
        if (max == waterfallMax) {
            return;
        }
        waterfallMax = max;
        if (_fullUpdate) { updateWaterfallFb(); };
    }

    float WaterFall::getWaterfallMax() {
        return waterfallMax;
    }

    void WaterFall::updateAllVFOs(bool checkRedrawRequired) {
        for (auto const& [name, vfo] : vfos) {
            if (checkRedrawRequired && !vfo->redrawRequired) { continue; }
            vfo->updateDrawingVars(viewBandwidth, dataWidth, viewOffset, widgetPos, fftHeight);
            vfo->wfRectMin = ImVec2(vfo->rectMin.x, wfMin.y);
            vfo->wfRectMax = ImVec2(vfo->rectMax.x, wfMax.y);
            vfo->wfLineMin = ImVec2(vfo->lineMin.x, wfMin.y - 1);
            vfo->wfLineMax = ImVec2(vfo->lineMax.x, wfMax.y - 1);
            vfo->wfLbwSelMin = ImVec2(vfo->wfRectMin.x - 2, vfo->wfRectMin.y);
            vfo->wfLbwSelMax = ImVec2(vfo->wfRectMin.x + 2, vfo->wfRectMax.y);
            vfo->wfRbwSelMin = ImVec2(vfo->wfRectMax.x - 2, vfo->wfRectMin.y);
            vfo->wfRbwSelMax = ImVec2(vfo->wfRectMax.x + 2, vfo->wfRectMax.y);
            vfo->redrawRequired = false;
        }
    }

    void WaterFall::setRawFFTSize(int size) {
        std::lock_guard<std::recursive_mutex> lck(buf_mtx);
        rawFFTSize = size;
        int wfSize = std::max<int>(1, waterfallHeight);
        if (rawFFTs != NULL) {
            rawFFTs = (float*)realloc(rawFFTs, rawFFTSize * wfSize * sizeof(float));
        }
        else {
            rawFFTs = (float*)malloc(rawFFTSize * wfSize * sizeof(float));
        }
        fftLines = 0;
        memset(rawFFTs, 0, rawFFTSize * waterfallHeight * sizeof(float));
        updateWaterfallFb();
    }

    void WaterFall::setBandPlanPos(int pos) {
        bandPlanPos = pos;
    }

    void WaterFall::setFFTHold(bool hold) {
        fftHold = hold;
        if (fftHold && latestFFTHold) {
            for (int i = 0; i < dataWidth; i++) {
                latestFFTHold[i] = -1000.0;
            }
        }
    }

    void WaterFall::setFFTHoldSpeed(float speed) {
        fftHoldSpeed = speed;
    }

    float* WaterFall::acquireLatestFFT(int& width) {
        latestFFTMtx.lock();
        if (!latestFFT) {
            latestFFTMtx.unlock();
            return NULL;
        }
        width = dataWidth;
        return latestFFT;
    }

    void WaterFall::releaseLatestFFT() {
        latestFFTMtx.unlock();
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
        bandwidthChanged = true;
        redrawRequired = true;
    }

    void WaterfallVFO::setReference(int ref) {
        if (reference == ref || ref < 0 || ref >= _REF_COUNT) {
            return;
        }
        reference = ref;
        setOffset(generalOffset);
    }

    void WaterfallVFO::setNotchOffset(double offset) {
        notchOffset = offset;
        redrawRequired = true;
    }

    void WaterfallVFO::setNotchVisible(bool visible) {
        notchVisible = visible;
        redrawRequired = true;
    }

    void WaterfallVFO::updateDrawingVars(double viewBandwidth, float dataWidth, double viewOffset, ImVec2 widgetPos, int fftHeight) {
        int center = roundf((((centerOffset - viewOffset) / (viewBandwidth / 2.0)) + 1.0) * ((double)dataWidth / 2.0));
        int left = roundf((((lowerOffset - viewOffset) / (viewBandwidth / 2.0)) + 1.0) * ((double)dataWidth / 2.0));
        int right = roundf((((upperOffset - viewOffset) / (viewBandwidth / 2.0)) + 1.0) * ((double)dataWidth / 2.0));
        int notch = roundf((((notchOffset + centerOffset - viewOffset) / (viewBandwidth / 2.0)) + 1.0) * ((double)dataWidth / 2.0));

        // Check weather the line is visible
        if (left >= 0 && left < dataWidth && reference == REF_LOWER) {
            lineVisible = true;
        }
        else if (center >= 0 && center < dataWidth && reference == REF_CENTER) {
            lineVisible = true;
        }
        else if (right >= 0 && right < dataWidth && reference == REF_UPPER) {
            lineVisible = true;
        }
        else {
            lineVisible = false;
        }

        // Calculate the position of the line
        if (reference == REF_LOWER) {
            lineMin = ImVec2(gui::waterfall.fftAreaMin.x + left, gui::waterfall.fftAreaMin.y);
            lineMax = ImVec2(gui::waterfall.fftAreaMin.x + left, gui::waterfall.fftAreaMax.y - 1);
        }
        else if (reference == REF_CENTER) {
            lineMin = ImVec2(gui::waterfall.fftAreaMin.x + center, gui::waterfall.fftAreaMin.y);
            lineMax = ImVec2(gui::waterfall.fftAreaMin.x + center, gui::waterfall.fftAreaMax.y - 1);
        }
        else if (reference == REF_UPPER) {
            lineMin = ImVec2(gui::waterfall.fftAreaMin.x + right, gui::waterfall.fftAreaMin.y);
            lineMax = ImVec2(gui::waterfall.fftAreaMin.x + right, gui::waterfall.fftAreaMax.y - 1);
        }

        int _left = left;
        int _right = right;
        left = std::clamp<int>(left, 0, dataWidth - 1);
        right = std::clamp<int>(right, 0, dataWidth - 1);
        leftClamped = (left != _left);
        rightClamped = (right != _right);

        rectMin = ImVec2(gui::waterfall.fftAreaMin.x + left, gui::waterfall.fftAreaMin.y + 1);
        rectMax = ImVec2(gui::waterfall.fftAreaMin.x + right + 1, gui::waterfall.fftAreaMax.y);

        float gripSize = 2.0f * style::uiScale;
        lbwSelMin = ImVec2(rectMin.x - gripSize, rectMin.y);
        lbwSelMax = ImVec2(rectMin.x + gripSize, rectMax.y);
        rbwSelMin = ImVec2(rectMax.x - gripSize, rectMin.y);
        rbwSelMax = ImVec2(rectMax.x + gripSize, rectMax.y);

        notchMin = ImVec2(gui::waterfall.fftAreaMin.x + notch - gripSize, gui::waterfall.fftAreaMin.y);
        notchMax = ImVec2(gui::waterfall.fftAreaMin.x + notch + gripSize, gui::waterfall.fftAreaMax.y - 1);
    }

    void WaterfallVFO::draw(ImGuiWindow* window, bool selected) {
        window->DrawList->AddRectFilled(rectMin, rectMax, color);
        if (lineVisible) {
            window->DrawList->AddLine(lineMin, lineMax, selected ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 255, 0, 255), style::uiScale);
        }

        if (notchVisible) {
            window->DrawList->AddRectFilled(notchMin, notchMax, IM_COL32(255, 0, 0, 127));
        }

        if (!gui::mainWindow.lockWaterfallControls && !gui::waterfall.inputHandled) {
            ImVec2 mousePos = ImGui::GetMousePos();
            if (rectMax.x - rectMin.x < 10) { return; }
            if (reference != REF_LOWER && !bandwidthLocked && !leftClamped) {
                if (IS_IN_AREA(mousePos, lbwSelMin, lbwSelMax)) { ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW); }
                else if (IS_IN_AREA(mousePos, wfLbwSelMin, wfLbwSelMax)) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                }
            }
            if (reference != REF_UPPER && !bandwidthLocked && !rightClamped) {
                if (IS_IN_AREA(mousePos, rbwSelMin, rbwSelMax)) { ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW); }
                else if (IS_IN_AREA(mousePos, wfRbwSelMin, wfRbwSelMax)) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                }
            }
        }
    };

    void WaterFall::showWaterfall() {
        buf_mtx.lock();
        if (rawFFTs == NULL) {
            spdlog::error("Null rawFFT");
        }
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
