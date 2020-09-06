#pragma once
#include <imgui/imgui.h>
#include <stdint.h>
#include <GL/glew.h>
#include <config.h>

namespace icons {
    extern ImTextureID LOGO;
    extern ImTextureID PLAY;
    extern ImTextureID STOP;
    extern ImTextureID MENU;

    GLuint loadTexture(char* path);
    void load();
}