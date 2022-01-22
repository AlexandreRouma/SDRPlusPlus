#pragma once
#include <imgui/imgui.h>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif
#include <GL/gl.h>

namespace icons {
    extern ImTextureID LOGO;
    extern ImTextureID PLAY;
    extern ImTextureID STOP;
    extern ImTextureID MENU;
    extern ImTextureID MUTED;
    extern ImTextureID UNMUTED;
    extern ImTextureID NORMAL_TUNING;
    extern ImTextureID CENTER_TUNING;

    GLuint loadTexture(std::string path);
    bool load(std::string resDir);
}