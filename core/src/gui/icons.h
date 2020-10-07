#pragma once
#include <imgui/imgui.h>
#include <GL/glew.h>
#include <string>

namespace icons {
    extern ImTextureID LOGO;
    extern ImTextureID PLAY;
    extern ImTextureID STOP;
    extern ImTextureID MENU;

    GLuint loadTexture(std::string path);
    void load();
}