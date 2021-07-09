#pragma once
#include <string>
#include <vector>
#include <portaudio.h>
#include <audio_device.h>


class PortaudioInterface {
public:
    PortaudioInterface();
    ~PortaudioInterface();
    static std::vector<AudioDevice_t> getDeviceList();

    template <typename RingBufferT, typename AudioObjectT>
    bool open(const AudioDevice_t &device, RingBufferT *ring_buffer, float sampleRate);

    void close();

private:
    static std::vector<float> getValidSampleRates(int channelCount, int deviceId, PaTime latency);

    template <typename RingBufferT, typename AudioObjectT>
    static int call_back(
            const void *inputBuffer,
            void *outputBuffer,
            unsigned long framesPerBuffer,
            const PaStreamCallbackTimeInfo* timeInfo,
            PaStreamCallbackFlags statusFlags,
            void *userData
            ) noexcept;

    PaStream *stream;
};