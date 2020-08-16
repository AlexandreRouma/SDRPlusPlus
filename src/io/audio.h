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
        enum {
            MONO,
            STEREO,
            _TYPE_COUNT
        };

        struct AudioDevice_t {
            std::string name;
            int index;
            int channels;
            std::vector<float> sampleRates;
            std::string txtSampleRates;
        };

        AudioSink() {
            
        }

        AudioSink(int bufferSize) {
            _bufferSize = bufferSize;
            monoBuffer = new float[_bufferSize];
            stereoBuffer = new dsp::StereoFloat_t[_bufferSize];
            _volume = 1.0f;
            Pa_Initialize();

            devTxtList = "";
            int devCount = Pa_GetDeviceCount();
            devIndex = Pa_GetDefaultOutputDevice();
            const PaDeviceInfo *deviceInfo;
            PaStreamParameters outputParams;
            outputParams.sampleFormat = paFloat32;
            outputParams.hostApiSpecificStreamInfo = NULL;
            for(int i = 0; i < devCount; i++) {
                deviceInfo = Pa_GetDeviceInfo(i);
                if (deviceInfo->maxOutputChannels < 1) {
                    continue;
                }
                AudioDevice_t dev;
                dev.name = deviceInfo->name;
                dev.index = i;
                dev.channels = std::min<int>(deviceInfo->maxOutputChannels, 2);
                dev.sampleRates.clear();
                dev.txtSampleRates = "";
                for (int j = 0; j < 6; j++) {
                    outputParams.channelCount = dev.channels;
                    outputParams.device = dev.index;
                    outputParams.suggestedLatency = deviceInfo->defaultLowOutputLatency;
                    PaError err = Pa_IsFormatSupported(NULL, &outputParams, POSSIBLE_SAMP_RATE[j]);
                    if (err != paFormatIsSupported) {
                        continue;
                    }
                    dev.sampleRates.push_back(POSSIBLE_SAMP_RATE[j]);
                    dev.txtSampleRates += std::to_string((int)POSSIBLE_SAMP_RATE[j]);
                    dev.txtSampleRates += '\0';
                }
                if (dev.sampleRates.size() == 0) {
                    continue;
                }
                if (i == devIndex) {
                    devListIndex = devices.size();
                    defaultDev = devListIndex;
                }
                devices.push_back(dev);
                deviceNames.push_back(deviceInfo->name);
                devTxtList += deviceInfo->name;
                devTxtList += '\0';
            }
        }

        void init(int bufferSize) {
            _bufferSize = bufferSize;
            monoBuffer = new float[_bufferSize];
            stereoBuffer = new dsp::StereoFloat_t[_bufferSize];
            _volume = 1.0f;
            Pa_Initialize();

            devTxtList = "";
            int devCount = Pa_GetDeviceCount();
            devIndex = Pa_GetDefaultOutputDevice();
            const PaDeviceInfo *deviceInfo;
            PaStreamParameters outputParams;
            outputParams.sampleFormat = paFloat32;
            outputParams.hostApiSpecificStreamInfo = NULL;
            for(int i = 0; i < devCount; i++) {
                deviceInfo = Pa_GetDeviceInfo(i);
                if (deviceInfo->maxOutputChannels < 1) {
                    continue;
                }
                AudioDevice_t dev;
                dev.name = deviceInfo->name;
                dev.index = i;
                dev.channels = std::min<int>(deviceInfo->maxOutputChannels, 2);
                dev.sampleRates.clear();
                dev.txtSampleRates = "";
                for (int j = 0; j < 6; j++) {
                    outputParams.channelCount = dev.channels;
                    outputParams.device = dev.index;
                    outputParams.suggestedLatency = deviceInfo->defaultLowOutputLatency;
                    PaError err = Pa_IsFormatSupported(NULL, &outputParams, POSSIBLE_SAMP_RATE[j]);
                    if (err != paFormatIsSupported) {
                        continue;
                    }
                    dev.sampleRates.push_back(POSSIBLE_SAMP_RATE[j]);
                    dev.txtSampleRates += std::to_string((int)POSSIBLE_SAMP_RATE[j]);
                    dev.txtSampleRates += '\0';
                }
                if (dev.sampleRates.size() == 0) {
                    continue;
                }
                if (i == devIndex) {
                    devListIndex = devices.size();
                    defaultDev = devListIndex;
                }
                devices.push_back(dev);
                deviceNames.push_back(deviceInfo->name);
                devTxtList += deviceInfo->name;
                devTxtList += '\0';
            }
        }

        void setMonoInput(dsp::stream<float>* input) {
            _monoInput = input;
        }

        void setStereoInput(dsp::stream<dsp::StereoFloat_t>* input) {
            _stereoInput = input;
        }

        void setVolume(float volume) {
            _volume = volume;
        }

        void start() {
            if (running) {
                return;
            }
            const PaDeviceInfo *deviceInfo;
            AudioDevice_t dev = devices[devListIndex];
            PaStreamParameters outputParams;
            deviceInfo = Pa_GetDeviceInfo(dev.index);
            outputParams.channelCount = 2;
            outputParams.sampleFormat = paFloat32;
            outputParams.hostApiSpecificStreamInfo = NULL;
            outputParams.device = dev.index;
            outputParams.suggestedLatency = Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;
            PaError err;
            if (streamType == MONO) {
                err = Pa_OpenStream(&stream, NULL, &outputParams, _sampleRate, _bufferSize, 0,
                                (dev.channels == 2) ? _mono_to_stereo_callback : _mono_to_mono_callback, this);
            }
            else {
                err = Pa_OpenStream(&stream, NULL, &outputParams, _sampleRate, _bufferSize, 0,
                                (dev.channels == 2) ? _stereo_to_stereo_callback : _stereo_to_mono_callback, this);
            }
            
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
            if (streamType == MONO) {
                _monoInput->stopReader();
            }
            else {
                _stereoInput->stopReader();
            }
            spdlog::warn("==> Pa_StopStream");
            Pa_StopStream(stream);
            spdlog::warn("==> Pa_CloseStream");
            Pa_CloseStream(stream);
            spdlog::warn("==> Done");
            if (streamType == MONO) {
                _monoInput->clearReadStop();
            }
            else {
                _stereoInput->clearReadStop();
            }
            running = false;
        }

        void setBlockSize(int blockSize) {
            if (running) {
                return;
            }
            _bufferSize = blockSize;
            delete[] monoBuffer;
            delete[] stereoBuffer;
            monoBuffer = new float[_bufferSize];
            stereoBuffer = new dsp::StereoFloat_t[_bufferSize];
        }

        void setSampleRate(float sampleRate) {
            _sampleRate = sampleRate;
        }

        void setDevice(int id) {
            if (devListIndex == id) {
                return;
            }
            if (running) {
                return;
            }
            devListIndex = id;
            devIndex = devices[id].index;
        }

        void setToDefault() {
            setDevice(defaultDev);
        }

        int getDeviceId() {
            return devListIndex;
        }

        void setStreamType(int type) {
            streamType = type;
        }

        std::string devTxtList;
        std::vector<AudioDevice_t> devices;
        std::vector<std::string> deviceNames;

    private:
        static int _mono_to_mono_callback(const void *input,
                                      void *output,
                                      unsigned long frameCount,
                                      const PaStreamCallbackTimeInfo* timeInfo, 
                                      PaStreamCallbackFlags statusFlags, void *userData ) {
            AudioSink* _this = (AudioSink*)userData;
            float* outbuf = (float*)output;
            if (_this->_monoInput->read(_this->monoBuffer, frameCount) < 0) {
                return 0;
            }
            
            float vol = powf(_this->_volume, 2);
            for (int i = 0; i < frameCount; i++) {
                outbuf[i] = _this->monoBuffer[i] * vol;
            }
            return 0;
        }

        static int _stereo_to_stereo_callback(const void *input,
                                      void *output,
                                      unsigned long frameCount,
                                      const PaStreamCallbackTimeInfo* timeInfo, 
                                      PaStreamCallbackFlags statusFlags, void *userData ) {
            AudioSink* _this = (AudioSink*)userData;
            dsp::StereoFloat_t* outbuf = (dsp::StereoFloat_t*)output;
            if (_this->_stereoInput->read(_this->stereoBuffer, frameCount) < 0) {
                return 0;
            }

            // Note: Calculate the power in the UI instead of here
            
            float vol = powf(_this->_volume, 2);
            for (int i = 0; i < frameCount; i++) {
                outbuf[i].l = _this->stereoBuffer[i].l * vol;
                outbuf[i].r = _this->stereoBuffer[i].r * vol;
            }
            return 0;
        }

        static int _mono_to_stereo_callback(const void *input,
                                      void *output,
                                      unsigned long frameCount,
                                      const PaStreamCallbackTimeInfo* timeInfo, 
                                      PaStreamCallbackFlags statusFlags, void *userData ) {
            AudioSink* _this = (AudioSink*)userData;
            dsp::StereoFloat_t* outbuf = (dsp::StereoFloat_t*)output;
            if (_this->_monoInput->read(_this->monoBuffer, frameCount) < 0) {
                return 0;
            }
            
            float vol = powf(_this->_volume, 2);
            for (int i = 0; i < frameCount; i++) {
                outbuf[i].l = _this->monoBuffer[i] * vol;
                outbuf[i].r = _this->monoBuffer[i] * vol;
            }
            return 0;
        }

        static int _stereo_to_mono_callback(const void *input,
                                      void *output,
                                      unsigned long frameCount,
                                      const PaStreamCallbackTimeInfo* timeInfo, 
                                      PaStreamCallbackFlags statusFlags, void *userData ) {
            AudioSink* _this = (AudioSink*)userData;
            float* outbuf = (float*)output;
            if (_this->_stereoInput->read(_this->stereoBuffer, frameCount) < 0) {
                return 0;
            }

            // Note: Calculate the power in the UI instead of here
            
            float vol = powf(_this->_volume, 2);
            for (int i = 0; i < frameCount; i++) {
                outbuf[i] = ((_this->stereoBuffer[i].l + _this->stereoBuffer[i].r) / 2.0f) * vol;
            }
            return 0;
        }

        float POSSIBLE_SAMP_RATE[6] = {
            48000.0f,
            44100.0f,
            24000.0f,
            22050.0f,
            12000.0f,
            11025.0f
        };

        int streamType;
        int devIndex;
        int devListIndex;
        int defaultDev;
        float _sampleRate;
        int _bufferSize;
        dsp::stream<float>* _monoInput;
        dsp::stream<dsp::StereoFloat_t>* _stereoInput;
        float* monoBuffer;
        dsp::StereoFloat_t* stereoBuffer;
        float _volume = 1.0f;
        PaStream *stream;
        bool running = false;
    };
};