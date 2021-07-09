#include "portaudio_interface.h"
#include <spdlog/spdlog.h>
#include <dsp/processing.h>

const std::array<float, 6> POSSIBLE_SAMPLE_RATES = {
        48000.0f,
        44100.0f,
        24000.0f,
        22050.0f,
        12000.0f,
        11025.0f
};

PortaudioInterface::PortaudioInterface() {
    spdlog::info("Initializing PortAudio. Version: {0}", Pa_GetVersionText());
    auto err = Pa_Initialize();
    if(err != PaErrorCode::paNoError) {
        spdlog::error("Failed to initialize PortAudio. Error code {0}: {1}", err, Pa_GetErrorText(err));
    }
    else {
        spdlog::info("PortAudio initialized successfully");
    }
}

PortaudioInterface::~PortaudioInterface() {
    auto err = Pa_Terminate();
    if(err != PaErrorCode::paNoError) {
        spdlog::error("Failed to terminate PortAudio. Error code {0}: {1}", err, Pa_GetErrorText(err));
    }
    else {
        spdlog::info("PortAudio closed successfully");
    }
}

std::vector<AudioDevice_t> PortaudioInterface::getDeviceList() {
    std::vector<AudioDevice_t> devices;

    auto numDevices = Pa_GetDeviceCount();
    for(int deviceId = 0; deviceId < numDevices ; deviceId++) {
        const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(deviceId);
        if (deviceInfo->maxOutputChannels < 1) {
            continue;
        }
        AudioDevice_t dev{
                deviceInfo->name,
                deviceId,
                std::min<int>(deviceInfo->maxOutputChannels, 2)};
        dev.sampleRates = std::move(getValidSampleRates(dev.channels, deviceId, deviceInfo->defaultLowOutputLatency));
        if(dev.sampleRates.empty()) continue;
        devices.push_back(dev);
    }

    return devices;
}

std::vector<float> PortaudioInterface::getValidSampleRates(int channelCount, int deviceId, PaTime latency) {
    std::vector<float> validSampleRates;

    PaStreamParameters outputParams;
    outputParams.sampleFormat = paFloat32;
    outputParams.hostApiSpecificStreamInfo = NULL;
    for (const auto sampleRate: POSSIBLE_SAMPLE_RATES) {
        outputParams.channelCount = channelCount;
        outputParams.device = deviceId;
        outputParams.suggestedLatency = latency;
        auto err = Pa_IsFormatSupported(NULL, &outputParams, sampleRate);
        if (err != paFormatIsSupported) {
            continue;
        }
        validSampleRates.push_back(sampleRate);
    }

    return validSampleRates;
}

template<typename BufferT, typename AudioObjectT>
int PortaudioInterface::call_back(const void *inputBuffer, void *outputBuffer, unsigned long framesPerBuffer,
                                  const PaStreamCallbackTimeInfo *timeInfo, PaStreamCallbackFlags statusFlags,
                                  void *userData) noexcept
{
    auto buffer = static_cast<BufferT *>(userData);
    int count = buffer->read();
    if (count < 0) {
        spdlog::warn("PortAudio nothing to read in the buffer!");
        return 0;
    }

    memcpy(outputBuffer, buffer->readBuf, framesPerBuffer * sizeof(AudioObjectT));
    buffer->flush();
    if (statusFlags & paOutputUnderflow) spdlog::warn("PortAudio underflow (read count: {0}, frames: {1})", count, framesPerBuffer);
    if (statusFlags & paOutputOverflow) spdlog::warn("PortAudio overflow");
    if (statusFlags & paPrimingOutput) spdlog::warn("PortAudio priming");

    if (count != framesPerBuffer) {  }

    return 0;
}

template<typename BufferT, typename AudioObjectT>
bool PortaudioInterface::open(const AudioDevice_t &device, BufferT *buffer, float sampleRate) {
    PaStreamParameters outputParams;
    auto deviceInfo = Pa_GetDeviceInfo(device.deviceId);
    outputParams.channelCount = device.channels;
    outputParams.sampleFormat = paFloat32;
    outputParams.hostApiSpecificStreamInfo = nullptr;
    outputParams.device = device.deviceId;
    outputParams.suggestedLatency = deviceInfo->defaultLowOutputLatency;
    int bufferFrames = (int)sampleRate / 60;
    auto err = Pa_OpenStream(
            &stream,
            nullptr,
            &outputParams,
            sampleRate,
            bufferFrames,
            0,
            call_back<BufferT, AudioObjectT>,
            buffer);

    if (err != PaErrorCode::paNoError) {
        spdlog::error("Failed to open PortAudio stream. Error code {0}: {1}", err, Pa_GetErrorText(err));
        return false;
    }

    err = Pa_StartStream(stream);

    if (err != PaErrorCode::paNoError) {
        spdlog::error("Failed to start PortAudio stream. Error code {0}: {1}", err, Pa_GetErrorText(err));
        return false;
    }

    return true;
}
template bool PortaudioInterface::open<dsp::stream<dsp::stereo_t>, dsp::stereo_t>(const AudioDevice_t &device, dsp::stream<dsp::stereo_t> *buffer, float sampleRate);
template bool PortaudioInterface::open<dsp::stream<dsp::mono_t>, dsp::mono_t>(const AudioDevice_t &device, dsp::stream<dsp::mono_t> *buffer, float sampleRate);

void PortaudioInterface::close() {
    Pa_StopStream(stream);
    Pa_CloseStream(stream);
    stream = nullptr;
}
