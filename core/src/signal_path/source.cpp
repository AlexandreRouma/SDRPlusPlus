#include <server.h>
#include <signal_path/source.h>
#include <utils/flog.h>
#include <signal_path/signal_path.h>
#include <core.h>

SourceManager::SourceManager() {
}

void SourceManager::registerSource(std::string name, SourceHandler* handler) {
    if (sources.find(name) != sources.end()) {
        flog::error("Tried to register new source with existing name: {0}", name);
        return;
    }
    sources[name] = handler;
    onSourceRegistered.emit(name);
}

void SourceManager::unregisterSource(std::string name) {
    if (sources.find(name) == sources.end()) {
        flog::error("Tried to unregister non existent source: {0}", name);
        return;
    }
    onSourceUnregister.emit(name);
    if (name == selectedName) {
        if (selectedHandler != NULL) {
            sources[selectedName]->deselectHandler(sources[selectedName]->ctx);
        }
        sigpath::iqFrontEnd.setInput(&nullSource);
        selectedHandler = NULL;
    }
    sources.erase(name);
    onSourceUnregistered.emit(name);
}

std::vector<std::string> SourceManager::getSourceNames() {
    std::vector<std::string> names;
    for (auto const& [name, src] : sources) { names.push_back(name); }
    return names;
}

void SourceManager::selectSource(std::string name) {
    if (sources.find(name) == sources.end()) {
        flog::error("Tried to select non existent source: {0}", name);
        return;
    }
    if (selectedHandler != NULL) {
        sources[selectedName]->deselectHandler(sources[selectedName]->ctx);
    }
    selectedHandler = sources[name];
    selectedHandler->selectHandler(selectedHandler->ctx);
    selectedName = name;
    if (core::args["server"].b()) {
        server::setInput(selectedHandler->stream);
    }
    else {
        sigpath::iqFrontEnd.setInput(selectedHandler->stream);
    }
    // Set server input here
}

void SourceManager::showSelectedMenu() {
    if (selectedHandler == NULL) {
        return;
    }
    selectedHandler->menuHandler(selectedHandler->ctx);
}

void SourceManager::start() {
    if (selectedHandler == NULL) {
        return;
    }
    selectedHandler->startHandler(selectedHandler->ctx);
}

void SourceManager::stop() {
    if (selectedHandler == NULL) {
        return;
    }
    selectedHandler->stopHandler(selectedHandler->ctx);
}

void SourceManager::tune(double freq) {
    if (selectedHandler == NULL) {
        return;
    }
    // TODO: No need to always retune the hardware in Panadapter mode
    selectedHandler->tuneHandler(((tuneMode == TuningMode::NORMAL) ? freq : ifFreq) + tuneOffset, selectedHandler->ctx);
    onRetune.emit(freq);
    currentFreq = freq;
}

void SourceManager::setTuningOffset(double offset) {
    tuneOffset = offset;
    tune(currentFreq);
}

void SourceManager::setTuningMode(TuningMode mode) {
    tuneMode = mode;
    tune(currentFreq);
}

void SourceManager::setPanadapterIF(double freq) {
    ifFreq = freq;
    tune(currentFreq);
}