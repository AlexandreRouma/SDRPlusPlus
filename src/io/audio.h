#pragma once
#include <thread>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <fstream>
#include <portaudio.h>
#include <spdlog/spdlog.h>

namespace io {
    class AudioSink {
    public:
        AudioSink() {
            
        }

        AudioSink(dsp::stream<float>* in, int bufferSize) {
            _bufferSize = bufferSize;
            _input = in;
            buffer = new float[_bufferSize * 2];
            _volume = 1.0f;
            Pa_Initialize();
        }

        void init(dsp::stream<float>* in, int bufferSize) {
            _bufferSize = bufferSize;
            _input = in;
            buffer = new float[_bufferSize * 2];
            _volume = 1.0f;
            Pa_Initialize();

            devTxtList = "";
            int devCount = Pa_GetDeviceCount();
            const PaDeviceInfo *deviceInfo;
            for(int i = 0; i < devCount; i++) {
                deviceInfo = Pa_GetDeviceInfo(i);
                devTxtList += deviceInfo->name;
                devTxtList += '\0';
            }
            devIndex = Pa_GetDefaultOutputDevice();
        }

        void setVolume(float volume) {
            _volume = volume;
        }

        void start() {
            if (running) {
                return;
            }
            PaStreamParameters outputParams;
            outputParams.channelCount = 2;
            outputParams.sampleFormat = paFloat32;
            outputParams.hostApiSpecificStreamInfo = NULL;
            outputParams.device = devIndex;
            outputParams.suggestedLatency = Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;
            PaError err = Pa_OpenStream(&stream, NULL, &outputParams, 48000.0f, _bufferSize, paClipOff, _callback, this);
            if (err != 0) {
                spdlog::error("Error while opening audio stream: ({0}) => {1}", err, Pa_GetErrorText(err));
                return;
            }
            err = Pa_StartStream(stream);
            if (err != 0) {
                spdlog::error("Error while starting audio stream: ({0}) => {1}", err, Pa_GetErrorText(err));
                return;
            }
            spdlog::info("Audio device open.");
            running = true;
        }

        void stop() {
            if (!running) {
                return;
            }
            Pa_StopStream(stream);
            Pa_CloseStream(stream);
            running = false;
        }

        void setBlockSize(int blockSize) {
            if (running) {
                return;
            }
            _bufferSize = blockSize;
        }

        void setDevice(int id) {
            if (devIndex == id) {
                return;
            }
            if (running) {
                return;
            }
            devIndex = id;
        }

        int getDeviceId() {
            return devIndex;
        }

        std::string devTxtList;

    private:
        static int _callback(const void *input,
                                      void *output,
                                      unsigned long frameCount,
                                      const PaStreamCallbackTimeInfo* timeInfo, 
                                      PaStreamCallbackFlags statusFlags, void *userData ) {
            AudioSink* _this = (AudioSink*)userData;
            float* outbuf = (float*)output;
            _this->_input->read(_this->buffer, frameCount);
            
            float vol = powf(_this->_volume, 2);
            for (int i = 0; i < frameCount; i++) {
                outbuf[(i * 2) + 0] = _this->buffer[i] * vol;
                outbuf[(i * 2) + 1] = _this->buffer[i] * vol;
            }
            return 0;
        }

        int devIndex;
        int _bufferSize;
        dsp::stream<float>* _input;
        float* buffer;
        float _volume;
        PaStream *stream;
        bool running = false;
    };
};