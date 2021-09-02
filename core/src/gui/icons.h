#pragma once
#include <imgui/imgui.h>
#include <GL/glew.h>
#include <string>

namespace icons {
    extern ImTextureID LOGO;
    extern ImTextureID PLAY;
    extern ImTextureID STOP;
    extern ImTextureID MENU;
    extern ImTextureID MUTED;
    extern ImTextureID UNMUTED;
    extern ImTextureID LEFTMUTED;
    extern ImTextureID LEFTUNMUTED;
    extern ImTextureID NORMAL_TUNING;
    extern ImTextureID CENTER_TUNING;
    extern ImTextureID LOCK;
    extern ImTextureID UNLOCK;

    GLuint loadTexture(std::string path);
    bool load(std::string resDir);
}