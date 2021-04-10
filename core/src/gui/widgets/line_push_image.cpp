#include <gui/widgets/line_push_image.h>

namespace ImGui {
    LinePushImage::LinePushImage(int frameWidth, int reservedIncrement) {
        _frameWidth = frameWidth;
        _reservedIncrement = reservedIncrement;
        frameBuffer = (uint8_t*)malloc(_frameWidth * _reservedIncrement * 4);
        reservedCount = reservedIncrement;

        glGenTextures(1, &textureId);
    }

    void LinePushImage::draw(const ImVec2& size_arg) {
        std::lock_guard<std::mutex> lck(bufferMtx);

        ImGuiWindow* window = GetCurrentWindow();
        ImGuiStyle& style = GetStyle();
        float pad = style.FramePadding.y;
        ImVec2 min = window->DC.CursorPos;

        // Calculate scale
        float width = CalcItemWidth();
        float height = roundf((width / (float)_frameWidth) * (float)_lineCount);

        ImVec2 size = CalcItemSize(size_arg, CalcItemWidth(), height);
        ImRect bb(min, ImVec2(min.x+size.x, min.y+size.y));
        float lineHeight = size.y;

        // If there are no lines, there is no point in drawing anything
        if (_lineCount == 0) { return; }

        ItemSize(size, style.FramePadding.y);
        if (!ItemAdd(bb, 0)) {
            return;
        }

        if (newData) {
            newData = false;
            updateTexture();
        }
        
        window->DrawList->AddImage((void*)(intptr_t)textureId, min, ImVec2(min.x + width, min.y + height));
    }

    uint8_t* LinePushImage::aquireNextLine(int count) {
        bufferMtx.lock();

        int oldLineCount = _lineCount;
        _lineCount += count;

        // If new data either fills up or excedes the limit, reallocate
        // TODO: Change it to avoid bug if count >= reservedIncrement
        if (_lineCount > reservedCount) {
            printf("Reallocating\n");
            reservedCount += _reservedIncrement;
            frameBuffer = (uint8_t*)realloc(frameBuffer, _frameWidth * reservedCount * 4);
        }

        return &frameBuffer[_frameWidth * oldLineCount * 4];
    }

    void LinePushImage::releaseNextLine() {
        newData = true;
        bufferMtx.unlock();
    }

    void LinePushImage::clear() {
        std::lock_guard<std::mutex> lck(bufferMtx);
        _lineCount = 0;
        frameBuffer = (uint8_t*)realloc(frameBuffer, _frameWidth * _reservedIncrement * 4);
        reservedCount = _reservedIncrement;
        newData = true;
    }

    void LinePushImage::save(std::string path) {
        // TODO: Implement
    }

    int LinePushImage::getLineCount() {
        return _lineCount;
    }

    void LinePushImage::updateTexture() {
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _frameWidth, _lineCount, 0, GL_RGBA, GL_UNSIGNED_BYTE, frameBuffer);
    }

}