#pragma once
#include <string>
#include <module.h>

namespace options {
    struct CMDLineOptions {
        std::string root;
        bool showConsole;
    };

    SDRPP_EXPORT CMDLineOptions opts;

    void loadDefaults();
    bool parse(int argc, char *argv[]);
}