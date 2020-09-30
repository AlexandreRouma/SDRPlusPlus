#pragma once
#include <string>
#include <vector>
#include <map>
#include <dsp/stream.h>
#include <dsp/types.h>

class SourceManager {
public:
    SourceManager();

    struct SourceHandler {
        dsp::stream<dsp::complex_t>* stream;
        void (*menuHandler)(void* ctx);
        void (*selectHandler)(void* ctx);
        void (*deselectHandler)(void* ctx);
        void (*startHandler)(void* ctx);
        void (*stopHandler)(void* ctx);
        void (*tuneHandler)(double freq, void* ctx);
        void* ctx;
    };

    void registerSource(std::string name, SourceHandler* handler);
    void selectSource(std::string  name);
    void showSelectedMenu();
    void start();
    void stop();
    void tune(double freq);

    std::vector<std::string> sourceNames;

private:
    std::map<std::string, SourceHandler*> sources;
    std::string selectedName;
    SourceHandler* selectedHandler = NULL;

};