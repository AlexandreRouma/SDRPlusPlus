#include <signal_path/source.h>
#include <spdlog/spdlog.h>
#include <signal_path/signal_path.h>

SourceManager::SourceManager() {

}

void SourceManager::registerSource(std::string name, SourceHandler* handler) {
    if (sources.find(name) != sources.end()) {
        spdlog::error("Tried to register new source with existing name: {0}", name);
        return;
    }
    sources[name] = handler;
    sourceNames.push_back(name);
}

void SourceManager::selectSource(std::string  name) {
    if (sources.find(name) == sources.end()) {
        spdlog::error("Tried to select non existant source: {0}", name);
        return;
    }
    if (selectedName != "") {
        sources[selectedName]->deselectHandler(sources[selectedName]->ctx);
    }
    selectedHandler = sources[name];
    selectedHandler->selectHandler(selectedHandler->ctx);
    selectedName = name;
    sigpath::signalPath.setInput(selectedHandler->stream);
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
    selectedHandler->tuneHandler(freq, selectedHandler->ctx);
}