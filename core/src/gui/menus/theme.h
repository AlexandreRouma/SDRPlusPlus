#pragma once
#include <string>

namespace thememenu {
    void init(std::string resDir);
    void applyTheme();
    void draw(void* ctx);
}