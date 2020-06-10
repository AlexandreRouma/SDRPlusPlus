#pragma once
#include <thread>
#include <cdsp/stream.h>
#include <cdsp/types.h>
#include <fstream>

namespace cdsp {
    #pragma pack(push, 1)
    struct audio_sample_t {
        int16_t l;
        int16_t r;
    };
    #pragma pack(pop)

    class RawFileSource {
    public:
        RawFileSource(std::string path, int bufferSize) : output(bufferSize * 2) {
            _bufferSize = bufferSize;
            _file = std::ifstream(path.c_str(), std::ios::in | std::ios::binary);
        }

        void start() {
            _workerThread = std::thread(_worker, this);
        }

        stream<float> output;

    private:
        static void _worker(RawFileSource* _this) {
            audio_sample_t* inBuf = new audio_sample_t[_this->_bufferSize];
            float* outBuf = new float[_this->_bufferSize];
            while (true) {
                //printf("%d\n", _this->_bufferSize * sizeof(audio_sample_t));
                _this->_file.read((char*)inBuf, _this->_bufferSize * sizeof(audio_sample_t));
                for (int i = 0; i < _this->_bufferSize; i++) {
                    outBuf[i] = ((float)inBuf[i].l + (float)inBuf[i].r) / (float)0xFFFF;
                }
                //printf("Writing file samples\n");
                _this->output.write(outBuf, _this->_bufferSize);
            }
        }

        int _bufferSize;
        std::ifstream _file;
        std::thread _workerThread;
    };




    class RawFileSink {
    public:
        RawFileSink(std::string path, stream<float>* in, int bufferSize) {
            _bufferSize = bufferSize;
            _input = in;
            _file = std::ofstream(path.c_str(), std::ios::out | std::ios::binary);
        }

        void start() {
            _workerThread = std::thread(_worker, this);
        }

    private:
        static void _worker(RawFileSink* _this) {
            float* inBuf = new float[_this->_bufferSize];
            audio_sample_t* outBuf = new audio_sample_t[_this->_bufferSize];
            while (true) {
                //printf("%d\n", _this->_bufferSize * sizeof(audio_sample_t));
                _this->_input->read(inBuf, _this->_bufferSize);
                for (int i = 0; i < _this->_bufferSize; i++) {
                    outBuf[i].l = inBuf[i] * 0x7FFF;
                    outBuf[i].r = outBuf[i].l;
                }
                //printf("Writing file samples\n");
                _this->_file.write((char*)outBuf, _this->_bufferSize * sizeof(audio_sample_t));
            }
        }

        int _bufferSize;
        std::ofstream _file;
        stream<float>* _input;
        std::thread _workerThread;
    };
};