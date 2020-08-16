#include <icons.h>

#define STB_IMAGE_IMPLEMENTATION
#include <imgui/stb_image.h>

namespace icons {
    ImTextureID LOGO;
    ImTextureID PLAY;
    ImTextureID STOP;
    ImTextureID PLAY_RAW;
    ImTextureID STOP_RAW;

    GLuint loadTexture(char* path) {
        int w,h,n;
        stbi_uc* data = stbi_load(path, &w, &h, &n, NULL);
        GLuint texId;
        glGenTextures(1, &texId);
        glBindTexture(GL_TEXTURE_2D, texId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, (uint8_t*)data);
        stbi_image_free(data);
        return texId;
    }

    void load() {
        LOGO = (ImTextureID)loadTexture("res/icons/sdrpp.png");
        PLAY = (ImTextureID)loadTexture("res/icons/play.png");
        STOP = (ImTextureID)loadTexture("res/icons/stop.png");
        PLAY_RAW = (ImTextureID)loadTexture("res/icons/play_raw.png");
        STOP_RAW = (ImTextureID)loadTexture("res/icons/stop_raw.png");
    }
}