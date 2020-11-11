#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <signal_path/sink.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

class AudioSink : SinkManager::Sink {
public:
    AudioSink(SinkManager::Stream* stream) {
        _stream = stream;
    }

    ~AudioSink() {

    }
    
    void menuHandler() {

    }

private:
    SinkManager::Stream* _stream;

};

class AudioSinkModule {
public:
    AudioSinkModule(std::string name) {
        this->name = name;
        provider.create = create_sink;
        provider.ctx = this;
        sigpath::sinkManager.registerSinkProvider("Audio", provider);
    }

    ~AudioSinkModule() {

    }

private:
    static SinkManager::Sink* create_sink(SinkManager::Stream* stream, void* ctx) {
        return (SinkManager::Sink*)(new AudioSink(stream));
    }

    std::string name;
    SinkManager::SinkProvider provider;

};

MOD_EXPORT void _INIT_() {
    // Nothing here
}

MOD_EXPORT void* _CREATE_INSTANCE_(std::string name) {
    AudioSinkModule* instance = new AudioSinkModule(name);
    return instance;
}

MOD_EXPORT void _DELETE_INSTANCE_() {
    
}

MOD_EXPORT void _STOP_() {
    
}