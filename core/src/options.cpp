#include <options.h>
#include <spdlog/spdlog.h>
#include <stdlib.h>
#include <filesystem>

namespace options {
    CMDLineOptions opts;

    void loadDefaults() {
#if defined(_WIN32)
        opts.root = ".";
        opts.showConsole = false;
#elif defined(IS_MACOS_BUNDLE)
        std::string homedir = getenv("HOME");
        opts.root = homedir + "/Library/Application Support/sdrpp";
#elif defined(__ANDROID__)
        opts.root = "/storage/self/primary/sdrpp";
#else
        std::string homedir = getenv("HOME");
        opts.root = homedir + "/.config/sdrpp";
#endif
        opts.root = std::filesystem::absolute(opts.root).string();
        opts.serverHost = "0.0.0.0";
        opts.serverPort = 5259;
    }

    bool parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; i++) {
            char* arg = argv[i];
            if (!strcmp(arg, "-r") || !strcmp(arg, "--root")) {
                if (i == argc - 1) { return false; }
                opts.root = std::filesystem::absolute(argv[++i]).string();
            }
            else if (!strcmp(arg, "-s") || !strcmp(arg, "--show-console")) {
                opts.showConsole = true;
            }
            else if (!strcmp(arg, "--server")) {
                opts.serverMode = true;
            }
            else if (!strcmp(arg, "-a") || !strcmp(arg, "--addr")) {
                if (i == argc - 1) { return false; }
                opts.serverHost = argv[++i];
                opts.showConsole = true;
            }
            else if (!strcmp(arg, "-p") || !strcmp(arg, "--port")) {
                if (i == argc - 1) { return false; }
                sscanf(argv[++i], "%d", &opts.serverPort);
                opts.showConsole = true;
            }
            else {
                spdlog::error("Invalid command line option: {0}", arg);
                return false;
            }
        }
        return true;
    }
}