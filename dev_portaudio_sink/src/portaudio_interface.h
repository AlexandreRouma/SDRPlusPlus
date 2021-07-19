#pragma once
#include <string>
#include <vector>
#include <portaudio.h>
#include <audio_device.h>
#include <atomic>


class PortaudioInterface {
public:
    PortaudioInterface();
    ~PortaudioInterface();
    static int getDefaultDeviceId();
    static std::vector<AudioDevice_t> getDeviceList();

private:
    static std::vector<float> getValidSampleRates(int channelCount, int deviceId, PaTime latency);
};

class PortaudioStream {
public:
    template <typename RingBufferT, typename AudioObjectT>
    bool open(const AudioDevice_t &device, RingBufferT *ring_buffer, float sampleRate);
    void close();

    template <typename RingBufferT, typename AudioObjectT>
    static int call_back(
            const void *inputBuffer,
            void *outputBuffer,
            unsigned long framesPerBuffer,
            const PaStreamCallbackTimeInfo* timeInfo,
            PaStreamCallbackFlags statusFlags,
            void *userData
    ) noexcept;
private:
    PaStream *stream = nullptr;
};