#pragma once
#include <string>

namespace options {
    struct CMDLineOptions {
        std::string root;
        bool help;
    };

    CMDLineOptions opts;

    void parse(char** argv, int argc);
}
