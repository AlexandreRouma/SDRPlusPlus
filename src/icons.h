#pragma once
#include <imgui/imgui.h>
#include <stdint.h>
#include <GL/glew.h>

namespace icons {
    extern ImTextureID PLAY;
    extern ImTextureID STOP;
    extern ImTextureID PLAY_RAW;
    extern ImTextureID STOP_RAW;

    GLuint loadTexture(char* path);
    void load();
}