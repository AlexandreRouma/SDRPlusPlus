#pragma once
#include <thread>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <fstream>
#include <portaudio.h>

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
        }

        void setVolume(float volume) {
            _volume = volume;
        }

        void start() {
            PaStreamParameters outputParams;
            outputParams.channelCount = 2;
            outputParams.sampleFormat = paFloat32;
            outputParams.hostApiSpecificStreamInfo = NULL;
            outputParams.device = Pa_GetDefaultOutputDevice();
            outputParams.suggestedLatency = Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;
            PaError err = Pa_OpenStream(&stream, NULL, &outputParams, 48000.0f, _bufferSize, paClipOff, _callback, this);
            printf("%s\n", Pa_GetErrorText(err));
            err = Pa_StartStream(stream);
            printf("%s\n", Pa_GetErrorText(err));
        }

        void stop() {
            Pa_CloseStream(stream);
        }

        void setBlockSize(int blockSize) {
            stop();
            _bufferSize = blockSize;
            start();
        }

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

        int _bufferSize;
        dsp::stream<float>* _input;
        float* buffer;
        float _volume;
        PaStream *stream;
    };
};