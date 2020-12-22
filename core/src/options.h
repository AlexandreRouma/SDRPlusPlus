#pragma once
#include <string>
#include <new_module.h>

namespace options {
    struct CMDLineOptions {
        std::string root;
    };

    SDRPP_EXPORT CMDLineOptions opts;

    void loadDefaults();
    bool parse(int argc, char *argv[]);
}