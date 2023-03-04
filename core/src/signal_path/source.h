#pragma once
#include <string>
#include <functional>
#include <map>
#include <mutex>
#include <dsp/types.h>
#include <dsp/stream.h>
#include <utils/event.h>

enum TuningMode {
    TUNING_MODE_NORMAL,
    TUNING_MODE_PANADAPTER
};

class Source;

class SourceManager {
    friend Source;
public:
    /**
     * Register a source.
     * @param name Name of the source.
     * @param source Pointer to the source instance.
     */
    void registerSource(const std::string& name, Source* source);

    /**
     * Unregister a source.
     * @param name Name of the source.
     */
    void unregisterSource(const std::string& name);

    /**
     * Get a list of source names.
     * @return List of source names.
     */
    const std::vector<std::string>& getSourceNames();

    /**
     * Select a source.
     * @param name Name of the source.
     */
    void select(const std::string& name);

    /**
     * Get the name of the currently selected source.
     * @return Name of the source or empty if no source is selected.
     */
    const std::string& getSelected();
    
    /**
     * Start the radio.
     * @return True if the radio started successfully, false if not.
     */
    bool start();

    /**
     * Stop the radio.
     */
    void stop();

    /**
     * Check if the radio is running.
     * @return True if the radio is running, false if not.
     */
    bool isRunning();

    /**
     * Tune the radio.
     * @param freq Frequency in Hz.
     */
    void tune(double freq);

    /**
     * Tune the radio.
     * @param freq Frequency to tune the radio to.
     */
    void showMenu();

    /**
     * Get the current samplerate of the radio.
     * @return Samplerate in Hz.
     */
    double getSamplerate();

    // =========== TODO: These functions should not happen in this class ===========

    /**
     * Set offset to add to the tuned frequency.
     * @param offset Offset in Hz.
     */
    void setTuningOffset(double offset);

    /**
     * Set tuning mode.
     * @param mode Tuning mode.
     */
    void setTuningMode(TuningMode mode);

    /**
     * Set panadapter mode IF frequency.
     * @param freq IF frequency in Hz.
     */
    void setPanadpterIF(double freq);

    // =============================================================================

    // Emitted after a new source has been registered.
    Event<std::string> onSourceRegistered;

    // Emitted when a source is about to be unregistered.
    Event<std::string> onSourceUnregister;

    // Emitted after a source has been unregistered.
    Event<std::string> onSourceUnregistered;

    // Emitted when the samplerate of the incoming IQ has changed.
    Event<double> onSamplerateChanged;

    // Emitted when the source manager is instructed to tune the radio.
    Event<double> onRetune;

private:
    void deselect();
    void setSamplerate(double samplerate);
    
    std::vector<std::string> sourceNames;
    std::map<std::string, Source*> sources;

    std::string selectedSourceName = "";
    Source* selectedSource = NULL;

    bool running = false;
    double samplerate = 1e6;
    double frequency = 100e6;
    double offset = 0;
    double ifFrequency = 8.830e6;
    TuningMode mode = TUNING_MODE_NORMAL;

    std::recursive_mutex mtx;
};

class Source {
public:
    virtual void showMenu() {}
    virtual void select() = 0;
    virtual void deselect() {}
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void tune(double freq) {}

    dsp::stream<dsp::complex_t> stream;
};