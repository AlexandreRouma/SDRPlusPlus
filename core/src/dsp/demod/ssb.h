#pragma once
#include "../processor.h"
#include "../channel/frequency_xlator.h" 
#include "../convert/complex_to_real.h"
#include "../loop/agc.h"

namespace dsp::demod {
    class SSB : public Processor<complex_t, float> {
        using base_type = Processor<complex_t, float>;
    public:
        enum Mode {
            USB,
            LSB,
            DSB
        };

        SSB() {}

        SSB(stream<complex_t>* in, Mode mode, double bandwidth, double samplerate, double agcAttack, double agcDecay) { init(in, mode, bandwidth, samplerate, agcAttack, agcDecay); }

        void init(stream<complex_t>* in, Mode mode, double bandwidth, double samplerate, double agcAttack, double agcDecay) {
            _mode = mode;
            _bandwidth = bandwidth;
            _samplerate = samplerate;
            xlator.init(NULL, getTranslation(), _samplerate);
            agc.init(NULL, 1.0, agcAttack, agcDecay, 10e6, 10.0);
            base_type::init(in);
        }

        void setMode(Mode mode) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _mode = mode;
            xlator.setOffset(getTranslation(), _samplerate);
            base_type::tempStart();
        }

        void setBandwidth(double bandwidth) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _bandwidth = bandwidth;
            xlator.setOffset(getTranslation(), _samplerate);
            base_type::tempStart();
        }

        void setSamplerate(double samplerate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _samplerate = samplerate;
            xlator.setOffset(getTranslation(), _samplerate);
            base_type::tempStart();
        }

        void setAGCAttack(double attack) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            agc.setAttack(attack);
        }

        void setAGCDecay(double decay) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            agc.setDecay(decay);
        }

        int process(int count, const complex_t* in, float* out) {
            // Move back sideband
            xlator.process(count, in, xlator.out.writeBuf);

            // Extract the real component
            convert::ComplexToReal::process(count, xlator.out.writeBuf, out);

            // Apply AGC
            agc.process(count, out, out);

            return count;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            base_type::_in->flush();
            if (!base_type::out.swap(count)) { return -1; }
            return count;
        }

    protected:
        double getTranslation() {
            if (_mode == Mode::USB) {
                return _bandwidth / 2.0;
            }
            else if (_mode == Mode::LSB) {
                return -_bandwidth / 2.0;
            }
            else if (_mode == Mode::DSB) {
                return 0.0;
            }
        }

        Mode _mode;
        double _bandwidth;
        double _samplerate;
        channel::FrequencyXlator xlator;
        loop::AGC<float> agc;

    };
};