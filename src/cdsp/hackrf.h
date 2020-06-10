#pragma once
#include <thread>
#include <cdsp/stream.h>
#include <cdsp/types.h>
#include <fstream>
#include <hackrf.h>
#include <Windows.h>

namespace cdsp {
    #pragma pack(push, 1)
    struct hackrf_sample_t {
        int8_t q;
        int8_t i;
    };
    #pragma pack(pop)

    class Complex2HackRF {
    public:
        Complex2HackRF() {
            
        }

        Complex2HackRF(stream<complex_t>* in, int bufferSize) : output(bufferSize * 2) {
            _input = in;
            _bufferSize = bufferSize;
        }

        void init(stream<complex_t>* in, int bufferSize) {
            output.init(bufferSize * 2);
            _input = in;
            _bufferSize = bufferSize;
        }

        stream<hackrf_sample_t> output;

        void start() {
            _workerThread = std::thread(_worker, this);
        }

    private:
        static void _worker(Complex2HackRF* _this) {
            complex_t* inBuf = new complex_t[_this->_bufferSize];
            hackrf_sample_t* outBuf = new hackrf_sample_t[_this->_bufferSize];
            while (true) {
                _this->_input->read(inBuf, _this->_bufferSize);
                for (int i = 0; i < _this->_bufferSize; i++) {
                    outBuf[i].i = inBuf[i].i * 127.0f;
                    outBuf[i].q = inBuf[i].q * 127.0f;
                }
                _this->output.write(outBuf, _this->_bufferSize);
            }
        }

        int _bufferSize;
        stream<complex_t>* _input;
        std::thread _workerThread;
    };

    class HackRF2Complex {
    public:
        HackRF2Complex() {
            
        }

        HackRF2Complex(stream<complex_t>* out, int bufferSize) : input(bufferSize * 2) {
            _output = out;
            _bufferSize = bufferSize;
        }

        void init(stream<complex_t>* out, int bufferSize) {
            input.init(bufferSize * 2);
            _output = out;
            _bufferSize = bufferSize;
        }

        void start() {
            _workerThread = std::thread(_worker, this);
        }

        stream<hackrf_sample_t> input;

    private:
        static void _worker(HackRF2Complex* _this) {
            hackrf_sample_t* inBuf = new hackrf_sample_t[_this->_bufferSize];
            complex_t* outBuf = new complex_t[_this->_bufferSize];
            while (true) {
                
                _this->input.read(inBuf, _this->_bufferSize);
                for (int i = 0; i < _this->_bufferSize; i++) {
                    outBuf[i].i = (float)inBuf[i].i / 127.0f;
                    outBuf[i].q = (float)inBuf[i].q / 127.0f;
                }
                _this->_output->write(outBuf, _this->_bufferSize);
            }
        }

        int _bufferSize;
        stream<complex_t>* _output;
        std::thread _workerThread;
    };

    class HackRFSink {
    public:
        HackRFSink() {

        }

        HackRFSink(hackrf_device* dev, int bufferSize, stream<complex_t>* input) : gen(input, bufferSize) {
            _input = input;
            _dev = dev;
            gen.start();
        }

        void init(hackrf_device* dev, int bufferSize, stream<complex_t>* input) {
            gen.init(input, bufferSize);
            _input = input;
            _dev = dev;
            gen.start();
        }

        void start() {
            streaming = true;
            hackrf_start_tx(_dev, _worker, this);
        }

        void stop() {
            streaming = false;
            Sleep(500);
            hackrf_stop_tx(_dev);
        }

    private:
        static int _worker(hackrf_transfer* transfer) {
            if (!((HackRFSink*)transfer->tx_ctx)->streaming) {
                return -1;
            }
            hackrf_sample_t* buf = (hackrf_sample_t*)transfer->buffer;
            ((HackRFSink*)transfer->tx_ctx)->gen.output.read(buf, transfer->buffer_length / 2);
            return 0;
        }
        
        Complex2HackRF gen;
        bool streaming;
        stream<complex_t>* _input;
        hackrf_device* _dev;
    };

    class HackRFSource {
    public:
        HackRFSource() {

        }

        HackRFSource(hackrf_device* dev, int bufferSize) : output(bufferSize * 2), gen(&output, bufferSize) {
            _dev = dev;
            gen.start();
        }

        void init(hackrf_device* dev, int bufferSize) {
            output.init(bufferSize * 2);
            gen.init(&output, bufferSize);
            _dev = dev;
            gen.start();
        }

        void start() {
            streaming = true;
            hackrf_start_rx(_dev, _worker, this);
        }

        void stop() {
            streaming = false;
            Sleep(500);
            hackrf_stop_rx(_dev);
        }

        stream<complex_t> output;

    private:
        static int _worker(hackrf_transfer* transfer) {
            if (!((HackRFSource*)transfer->rx_ctx)->streaming) {
                return -1;
            }
            hackrf_sample_t* buf = (hackrf_sample_t*)transfer->buffer;
            //printf("Writing samples\n");
            ((HackRFSource*)transfer->rx_ctx)->gen.input.write(buf, transfer->buffer_length / 2);
            return 0;
        }
        
        HackRF2Complex gen;
        bool streaming;
        hackrf_device* _dev;
    };
};