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

    GLuint loadTexture(std::string path);
    void load();
}