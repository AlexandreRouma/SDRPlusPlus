#include <gui/icons.h>
#include <stdint.h>
#include <GL/glew.h>
#include <config.h>
#include <options.h>

#define STB_IMAGE_IMPLEMENTATION
#include <imgui/stb_image.h>

namespace icons {
    ImTextureID LOGO;
    ImTextureID PLAY;
    ImTextureID STOP;
    ImTextureID MENU;
    ImTextureID MUTED;
    ImTextureID UNMUTED;
    ImTextureID NORMAL_TUNING;
    ImTextureID CENTER_TUNING;

    GLuint loadTexture(std::string path) {
        int w,h,n;
        stbi_uc* data = stbi_load(path.c_str(), &w, &h, &n, 0);
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
        LOGO = (ImTextureID)(uintptr_t)loadTexture(options::opts.root + "/res/icons/sdrpp.png");
        PLAY = (ImTextureID)(uintptr_t)loadTexture(options::opts.root + "/res/icons/play.png");
        STOP = (ImTextureID)(uintptr_t)loadTexture(options::opts.root + "/res/icons/stop.png");
        MENU = (ImTextureID)(uintptr_t)loadTexture(options::opts.root + "/res/icons/menu.png");
        MUTED = (ImTextureID)(uintptr_t)loadTexture(options::opts.root + "/res/icons/muted.png");
        UNMUTED = (ImTextureID)(uintptr_t)loadTexture(options::opts.root + "/res/icons/unmuted.png");
        NORMAL_TUNING = (ImTextureID)(uintptr_t)loadTexture(options::opts.root + "/res/icons/normal_tuning.png");
        CENTER_TUNING = (ImTextureID)(uintptr_t)loadTexture(options::opts.root + "/res/icons/center_tuning.png");
    }
}