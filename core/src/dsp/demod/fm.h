#pragma once
#include "../processor.h"
#include "quadrature.h"
#include "../filter/fir.h"
#include "../taps/low_pass.h"
#include "../convert/mono_to_stereo.h"

namespace dsp::demod {
    template <class T>
    class FM : public dsp::Processor<dsp::complex_t, T> {
        using base_type = dsp::Processor<dsp::complex_t, T>;
    public:
        FM() {}

        FM(dsp::stream<dsp::complex_t>* in, double samplerate, double bandwidth, bool lowPass) { init(in, samplerate, bandwidth, lowPass); }

        ~FM() {
            if (!base_type::_block_init) { return; }
            base_type::stop();
            dsp::taps::free(lpfTaps);
        }

        void init(dsp::stream<dsp::complex_t>* in, double samplerate, double bandwidth, bool lowPass) {
            _samplerate = samplerate;
            _bandwidth = bandwidth;
            _lowPass = lowPass;

            demod.init(NULL, bandwidth / 2.0, _samplerate);
            lpfTaps = dsp::taps::lowPass(_bandwidth / 2.0, (_bandwidth / 2.0) * 0.1, _samplerate);
            lpf.init(NULL, lpfTaps);

            if constexpr (std::is_same_v<T, float>) {
                demod.out.free();
            }
            lpf.out.free();

            base_type::init(in);
        }

        void setSamplerate(double samplerate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _samplerate = samplerate;
            demod.setDeviation(_bandwidth / 2.0, _samplerate);
            dsp::taps::free(lpfTaps);
            lpfTaps = dsp::taps::lowPass(_bandwidth / 2.0, (_bandwidth / 2.0) * 0.1, _samplerate);
            lpf.setTaps(lpfTaps);
            base_type::tempStart();
        }

        void setBandwidth(double bandwidth) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            if (bandwidth == _bandwidth) { return; }
            _bandwidth = bandwidth;
            std::lock_guard<std::mutex> lck2(lpfMtx);
            demod.setDeviation(_bandwidth / 2.0, _samplerate);
            dsp::taps::free(lpfTaps);
            lpfTaps = dsp::taps::lowPass(_bandwidth / 2, (_bandwidth / 2) * 0.1, _samplerate);
            lpf.setTaps(lpfTaps);
        }

        void setLowPass(bool lowPass) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            std::lock_guard<std::mutex> lck2(lpfMtx);
            _lowPass = lowPass;
            lpf.reset();
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            demod.reset();
            lpf.reset();
            base_type::tempStart();
        }

        inline int process(int count, dsp::complex_t* in, T* out) {
            if constexpr (std::is_same_v<T, float>) {
                demod.process(count, in, out);
                if (_lowPass) {
                    std::lock_guard<std::mutex> lck(lpfMtx);
                    lpf.process(count, out, out);
                }
            }
            if constexpr (std::is_same_v<T, stereo_t>) {
                demod.process(count, in, demod.out.writeBuf);
                if (_lowPass) {
                    std::lock_guard<std::mutex> lck(lpfMtx);
                    lpf.process(count, demod.out.writeBuf, demod.out.writeBuf);
                }
                convert::MonoToStereo::process(count, demod.out.writeBuf, out);
            }
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

    private:
        double _samplerate;
        double _bandwidth;
        bool _lowPass;

        Quadrature demod;
        tap<float> lpfTaps;
        filter::FIR<float, float> lpf;
        std::mutex lpfMtx;
    };
}