#include <gui/widgets/image.h>

namespace ImGui {
    ImageDisplay::ImageDisplay(int width, int height, GLenum format) {
        _width = width;
        _height = height;
        _format = format;
        buffer = malloc(_width * _height * 4);
        activeBuffer = malloc(_width * _height * 4);
        memset(buffer, 0, _width * _height * 4);
        memset(activeBuffer, 0, _width * _height * 4);

        glGenTextures(1, &textureId);
    }

    ImageDisplay::~ImageDisplay() {
        free(buffer);
        free(activeBuffer);
    }

    void ImageDisplay::draw(const ImVec2& size_arg) {
        std::lock_guard<std::mutex> lck(bufferMtx);

        ImGuiWindow* window = GetCurrentWindow();
        ImGuiStyle& style = GetStyle();
        float pad = style.FramePadding.y;
        ImVec2 min = window->DC.CursorPos;

        // Calculate scale
        float width = CalcItemWidth();
        float height = roundf((width / (float)_width) * (float)_height);

        ImVec2 size = CalcItemSize(size_arg, CalcItemWidth(), height);
        ImRect bb(min, ImVec2(min.x+size.x, min.y+size.y));
        float lineHeight = size.y;

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

    void ImageDisplay::swap() {
        std::lock_guard<std::mutex> lck(bufferMtx);
        void* tmp = activeBuffer;
        activeBuffer = buffer;
        buffer = tmp;
        newData = true;
        memset(buffer, 0, _width * _height * 4);
    }

    void ImageDisplay::updateTexture() {
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, _width, _height, 0, _format, GL_UNSIGNED_BYTE, activeBuffer);
    }
}