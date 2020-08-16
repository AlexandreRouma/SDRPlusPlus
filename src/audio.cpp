#include <audio.h>

namespace audio {
    std::map<std::string, AudioStream_t*> streams;

    float registerMonoStream(dsp::stream<float>* stream, std::string name, std::string vfoName, int (*sampleRateChangeHandler)(void* ctx, float sampleRate), void* ctx) {
        AudioStream_t* astr = new AudioStream_t;
        astr->type = STREAM_TYPE_MONO;
        astr->ctx = ctx;
        astr->audio = new io::AudioSink;
        astr->audio->init(1);
        astr->deviceId = astr->audio->getDeviceId();
        float sampleRate = astr->audio->devices[astr->deviceId].sampleRates[0];
        int blockSize = sampleRate / 200; // default block size
        astr->monoAudioStream = new dsp::stream<float>(blockSize * 2);
        astr->audio->setBlockSize(blockSize);
        astr->audio->setStreamType(io::AudioSink::MONO);
        astr->audio->setMonoInput(astr->monoAudioStream);
        astr->audio->setSampleRate(sampleRate);
        astr->blockSize = blockSize;
        astr->sampleRate = sampleRate;
        astr->monoStream = stream;
        astr->sampleRateChangeHandler = sampleRateChangeHandler;
        astr->monoDynSplit = new dsp::DynamicSplitter<float>(stream, blockSize);
        astr->monoDynSplit->bind(astr->monoAudioStream);
        astr->running = false;
        astr->volume = 1.0f;
        astr->sampleRateId = 0;
        astr->vfoName = vfoName;
        streams[name] = astr;
        return sampleRate;
    }

    float registerStereoStream(dsp::stream<dsp::StereoFloat_t>* stream, std::string name, std::string vfoName, int (*sampleRateChangeHandler)(void* ctx, float sampleRate), void* ctx) {
        AudioStream_t* astr = new AudioStream_t;
        astr->type = STREAM_TYPE_STEREO;
        astr->ctx = ctx;
        astr->audio = new io::AudioSink;
        astr->audio->init(1);
        float sampleRate = astr->audio->devices[astr->audio->getDeviceId()].sampleRates[0];
        int blockSize = sampleRate / 200; // default block size
        astr->stereoAudioStream = new dsp::stream<dsp::StereoFloat_t>(blockSize * 2);
        astr->audio->setBlockSize(blockSize);
        astr->audio->setStreamType(io::AudioSink::STEREO);
        astr->audio->setStereoInput(astr->stereoAudioStream);
        astr->audio->setSampleRate(sampleRate);
        astr->blockSize = blockSize;
        astr->sampleRate = sampleRate;
        astr->stereoStream = stream;
        astr->sampleRateChangeHandler = sampleRateChangeHandler;
        astr->stereoDynSplit = new dsp::DynamicSplitter<dsp::StereoFloat_t>(stream, blockSize);
        astr->stereoDynSplit->bind(astr->stereoAudioStream);
        astr->running = false;
        streams[name] = astr;
        astr->vfoName = vfoName;
        return sampleRate;
    }

    void startStream(std::string name) {
        AudioStream_t* astr = streams[name];
        if (astr->running) {
            return;
        }
        if (astr->type == STREAM_TYPE_MONO) {
            astr->monoDynSplit->start();
        }
        else {
            astr->stereoDynSplit->start();
        }
        astr->audio->start();
        astr->running = true;
    }

    void stopStream(std::string name) {
        AudioStream_t* astr = streams[name];
        if (!astr->running) {
            return;
        }
        if (astr->type == STREAM_TYPE_MONO) {
            spdlog::warn("=> Stopping monoDynSplit");
            astr->monoDynSplit->stop();
        }
        else {
            spdlog::warn("=> Stopping stereoDynSplit");
            astr->stereoDynSplit->stop();
        }
        spdlog::warn("=> Stopping audio");
        astr->audio->stop();
        spdlog::warn("=> Done");
        astr->running = false;
    }

    void removeStream(std::string name) {
        AudioStream_t* astr = streams[name];
        stopStream(name);
        for (int i = 0; i < astr->boundStreams.size(); i++) {
            astr->boundStreams[i].streamRemovedHandler(astr->ctx);
        }
        delete astr->monoDynSplit;
    }

    dsp::stream<float>* bindToStreamMono(std::string name, void (*streamRemovedHandler)(void* ctx), void (*sampleRateChangeHandler)(void* ctx, float sampleRate, int blockSize), void* ctx) {
        AudioStream_t* astr = streams[name];
        BoundStream_t bstr;
        bstr.type = STREAM_TYPE_MONO;
        bstr.ctx = ctx;
        bstr.streamRemovedHandler = streamRemovedHandler;
        bstr.sampleRateChangeHandler = sampleRateChangeHandler;
        if (astr->type == STREAM_TYPE_MONO) {
            bstr.monoStream = new dsp::stream<float>(astr->blockSize * 2);
            astr->monoDynSplit->bind(bstr.monoStream);
            return bstr.monoStream;
        }
        bstr.stereoStream = new dsp::stream<dsp::StereoFloat_t>(astr->blockSize * 2);
        bstr.s2m = new dsp::StereoToMono(bstr.stereoStream, astr->blockSize * 2);
        bstr.monoStream = &bstr.s2m->output;
        astr->stereoDynSplit->bind(bstr.stereoStream);
        bstr.s2m->start();
        return bstr.monoStream;
    }

    dsp::stream<dsp::StereoFloat_t>* bindToStreamStereo(std::string name, void (*streamRemovedHandler)(void* ctx), void (*sampleRateChangeHandler)(void* ctx, float sampleRate, int blockSize), void* ctx) {
        AudioStream_t* astr = streams[name];
        BoundStream_t bstr;
        bstr.type = STREAM_TYPE_STEREO;
        bstr.ctx = ctx;
        bstr.streamRemovedHandler = streamRemovedHandler;
        bstr.sampleRateChangeHandler = sampleRateChangeHandler;
        if (astr->type == STREAM_TYPE_STEREO) {
            bstr.stereoStream = new dsp::stream<dsp::StereoFloat_t>(astr->blockSize * 2);
            astr->stereoDynSplit->bind(bstr.stereoStream);
            return bstr.stereoStream;
        }
        bstr.monoStream = new dsp::stream<float>(astr->blockSize * 2);
        bstr.m2s = new dsp::MonoToStereo(bstr.monoStream, astr->blockSize * 2);
        bstr.stereoStream = &bstr.m2s->output;
        astr->monoDynSplit->bind(bstr.monoStream);
        bstr.m2s->start();
    }

    void setBlockSize(std::string name, int blockSize) {
        AudioStream_t* astr = streams[name];
        if (astr->running) {
            return;
        }
        if (astr->type == STREAM_TYPE_MONO) {
            astr->monoDynSplit->setBlockSize(blockSize);
            for (int i = 0; i < astr->boundStreams.size(); i++) {
                BoundStream_t bstr = astr->boundStreams[i];
                bstr.monoStream->setMaxLatency(blockSize * 2);
                if (bstr.type == STREAM_TYPE_STEREO) {
                    bstr.m2s->stop();
                    bstr.m2s->setBlockSize(blockSize);
                    bstr.m2s->start();
                }
            }
            astr->blockSize = blockSize;
            return;
        }
        astr->monoDynSplit->setBlockSize(blockSize);
        for (int i = 0; i < astr->boundStreams.size(); i++) {
            BoundStream_t bstr = astr->boundStreams[i];
            bstr.stereoStream->setMaxLatency(blockSize * 2);
            if (bstr.type == STREAM_TYPE_MONO) {
                bstr.s2m->stop();
                bstr.s2m->setBlockSize(blockSize);
                bstr.s2m->start();
            }
        }
        astr->blockSize = blockSize;
    }

    void unbindFromStreamMono(std::string name, dsp::stream<float>* stream) {
        AudioStream_t* astr = streams[name];
        for (int i = 0; i < astr->boundStreams.size(); i++) {
            BoundStream_t bstr = astr->boundStreams[i];
            if (bstr.monoStream != stream) {
                continue;
            }
            if (astr->type == STREAM_TYPE_STEREO) {
                bstr.s2m->stop();
                delete bstr.s2m;
            }
            delete stream;
            return;
        }
    }

    void unbindFromStreamStereo(std::string name, dsp::stream<dsp::StereoFloat_t>* stream) {
        AudioStream_t* astr = streams[name];
        for (int i = 0; i < astr->boundStreams.size(); i++) {
            BoundStream_t bstr = astr->boundStreams[i];
            if (bstr.stereoStream != stream) {
                continue;
            }
            if (astr->type == STREAM_TYPE_MONO) {
                bstr.s2m->stop();
                delete bstr.m2s;
            }
            delete stream;
            return;
        }
    }

    std::string getNameFromVFO(std::string vfoName) {
        for (auto const& [name, stream] : streams) {
            if (stream->vfoName == vfoName) {
                return name;
            }
        }
        return "";
    }

    void setSampleRate(std::string name, float sampleRate) {
        AudioStream_t* astr = streams[name];
        if (astr->running) {
            return;
        }
        int blockSize = astr->sampleRateChangeHandler(astr->ctx, sampleRate);
        astr->audio->setSampleRate(sampleRate);
        astr->audio->setBlockSize(blockSize);
        if (astr->type == STREAM_TYPE_MONO) {
            astr->monoDynSplit->setBlockSize(blockSize);
            for (int i = 0; i < astr->boundStreams.size(); i++) {
                BoundStream_t bstr = astr->boundStreams[i];
                if (bstr.type == STREAM_TYPE_STEREO) {
                    bstr.m2s->stop();
                    bstr.m2s->setBlockSize(blockSize);
                    bstr.sampleRateChangeHandler(bstr.ctx, sampleRate, blockSize);
                    bstr.m2s->start();
                    continue;
                }
                bstr.sampleRateChangeHandler(bstr.ctx, sampleRate, blockSize);
            }
        }
        else {
            astr->stereoDynSplit->setBlockSize(blockSize);
            for (int i = 0; i < astr->boundStreams.size(); i++) {
                BoundStream_t bstr = astr->boundStreams[i];
                if (bstr.type == STREAM_TYPE_MONO) {
                    bstr.s2m->stop();
                    bstr.s2m->setBlockSize(blockSize);
                    bstr.sampleRateChangeHandler(bstr.ctx, sampleRate, blockSize);
                    bstr.s2m->start();
                    continue;
                }
                bstr.sampleRateChangeHandler(bstr.ctx, sampleRate, blockSize);
            }
        }
    }

    void setAudioDevice(std::string name, int deviceId, float sampleRate) {
        AudioStream_t* astr = streams[name];
        if (astr->running) {
            return;
        }
        astr->deviceId = deviceId;
        astr->audio->setDevice(deviceId);
        setSampleRate(name, sampleRate);
    }
};

