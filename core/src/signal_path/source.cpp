#include "source.h"
#include <utils/flog.h>

void SourceManager::registerSource(const std::string& name, Source* source) {
    std::lock_guard<std::recursive_mutex> lck(mtx);

    // Check arguments
    if (source || name.empty()) {
        flog::error("Invalid argument to register source", name);
        return;
    }

    // Check that a source with that name doesn't already exist
    if (sources.find(name) != sources.end()) {
        flog::error("Tried to register source with existing name: {}", name);
        return;
    }

    // Add source to map
    sources[name] = source;

    // Add source to lists
    sourceNames.push_back(name);
    onSourceRegistered(name);
}

void SourceManager::unregisterSource(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lck(mtx);

    // Check that a source with that name exists
    if (sources.find(name) == sources.end()) {
        flog::error("Tried to unregister a non-existent source: {}", name);
        return;
    }

    // Notify event listeners of the imminent deletion
    onSourceUnregister(name);

    // Delete from lists
    sourceNames.erase(std::find(sourceNames.begin(), sourceNames.end(), name));
    sources.erase(name);

    // Notify event listeners of the deletion
    onSourceUnregistered(name);
}

const std::vector<std::string>& SourceManager::getSourceNames() {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    return sourceNames;
}

void SourceManager::select(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lck(mtx);

    // make sure that source isn't currently selected
    if (selectedSourceName == name) { return; }

    // Deselect current source
    deselect();

    // Check that a source with that name exists
    if (sources.find(name) == sources.end()) {
        flog::error("Tried to select a non-existent source: {}", name);
        return;
    }

    // Select the source
    selectedSourceName = name;
    selectedSource = sources[name];

    // Call the selected source
    selectedSource->select();

    // Retune to make sure the source has the latest frequency
    tune(frequency);
}

const std::string& SourceManager::getSelected() {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    return selectedSourceName;
}

bool SourceManager::start() {
    std::lock_guard<std::recursive_mutex> lck(mtx);

    // Check if not already running
    if (running) { return true; }

    // Call source if selected and save if started
    running = (!selectedSource) ? false : selectedSource->start();

    return running;
}

void SourceManager::stop() {
    std::lock_guard<std::recursive_mutex> lck(mtx);

    // Check if running
    if (!running) { return; }

    // Call source if selected and save state
    if (selectedSource) { selectedSource->stop(); }
    running = false;
}

bool SourceManager::isRunning() {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    return running;
}

void SourceManager::tune(double freq) {
    std::lock_guard<std::recursive_mutex> lck(mtx);

    // Save frequency
    frequency = freq;
    
    // Call source if selected
    if (selectedSource) {
        selectedSource->tune(((mode == TUNING_MODE_NORMAL) ? freq : ifFrequency) + offset);
    }
}

void SourceManager::showMenu() {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    
    // Call source if selected
    if (selectedSource) { selectedSource->showMenu(); }
}

double SourceManager::getSamplerate() {
    std::lock_guard<std::recursive_mutex> lck(mtx);
    return samplerate;
}

// =========== TODO: These functions should not happen in this class ===========

void SourceManager::setTuningOffset(double offset) {
    std::lock_guard<std::recursive_mutex> lck(mtx);

    // Update offset
    this->offset = offset;

    // Retune to take affect
    tune(frequency);
}

void SourceManager::setTuningMode(TuningMode mode) {
    std::lock_guard<std::recursive_mutex> lck(mtx);

    // Update mode
    this->mode = mode;

    // Retune to take affect
    tune(frequency);
}

void SourceManager::setPanadpterIF(double freq) {
    std::lock_guard<std::recursive_mutex> lck(mtx);

    // Update offset
    ifFrequency = freq;

    // Return to take affect if in panadapter mode 
    if (mode == TUNING_MODE_PANADAPTER) { tune(frequency); }
}

// =============================================================================

void SourceManager::deselect() {
    std::lock_guard<std::recursive_mutex> lck(mtx);

    // Call source if selected
    if (selectedSource) { selectedSource->deselect(); }

    // Mark as deselected
    selectedSourceName.clear();
    selectedSource = NULL;
}

void SourceManager::setSamplerate(double samplerate) {
    std::lock_guard<std::recursive_mutex> lck(mtx);

    // Save samplerate and emit event
    this->samplerate = samplerate;
    onSamplerateChanged(samplerate);
}