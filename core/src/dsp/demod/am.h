#pragma once
#include "../processor.h"
#include "../loop/agc.h"
#include "../correction/dc_blocker.h"
#include "../convert/mono_to_stereo.h"
#include "../filter/fir.h"
#include "../taps/low_pass.h"

namespace dsp::demod {
    template <class T>
    class AM : public Processor<dsp::complex_t, T> {
        using base_type = Processor<dsp::complex_t, T>;
    public:
        enum AGCMode {
            CARRIER,
            AUDIO,
        };

        AM() {}

        AM(stream<complex_t>* in, AGCMode agcMode, double bandwidth, double agcAttack, double agcDecay, double dcBlockRate, double samplerate) { init(in, agcMode, bandwidth, agcAttack, agcDecay, dcBlockRate, samplerate); }

        ~AM() {
            if (!base_type::_block_init) { return; }
            base_type::stop();
            taps::free(lpfTaps);
        }

        void init(stream<complex_t>* in, AGCMode agcMode, double bandwidth, double agcAttack, double agcDecay, double dcBlockRate, double samplerate) {
            _agcMode = agcMode;
            _bandwidth = bandwidth;
            _samplerate = samplerate;

            carrierAgc.init(NULL, 1.0, agcAttack, agcDecay, 10e6, 10.0, INFINITY);
            audioAgc.init(NULL, 1.0, agcAttack, agcDecay, 10e6, 10.0, INFINITY);
            dcBlock.init(NULL, dcBlockRate);
            lpfTaps = taps::lowPass(bandwidth / 2.0, (bandwidth / 2.0) * 0.1, samplerate);
            lpf.init(NULL, lpfTaps);

            if constexpr (std::is_same_v<T, float>) {
                audioAgc.out.free();
            }
            dcBlock.out.free();
            lpf.out.free();
            
            base_type::init(in);
        }

        void setAGCMode(AGCMode agcMode) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _agcMode = agcMode;
            reset();
            base_type::tempStart();
        }

        void setBandwidth(double bandwidth) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            if (bandwidth == _bandwidth) { return; }
            _bandwidth = bandwidth;
            std::lock_guard<std::mutex> lck2(lpfMtx);
            taps::free(lpfTaps);
            lpfTaps = taps::lowPass(_bandwidth / 2.0, (_bandwidth / 2.0) * 0.1, _samplerate);
            lpf.setTaps(lpfTaps);
        }

        void setAGCAttack(double attack) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            carrierAgc.setAttack(attack);
            audioAgc.setAttack(attack);
        }

        void setAGCDecay(double decay) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            carrierAgc.setDecay(decay);
            audioAgc.setDecay(decay);
        }

        void setDCBlockRate(double rate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            dcBlock.setRate(rate);
        }

        // TODO: Implement setSamplerate

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            carrierAgc.reset();
            audioAgc.reset();
            dcBlock.reset();
            base_type::tempStart();
        }

        int process(int count, complex_t* in, T* out) {
            // Apply carrier AGC if needed
            if (_agcMode == AGCMode::CARRIER) {
                carrierAgc.process(count, in, carrierAgc.out.writeBuf);
                in = carrierAgc.out.writeBuf;
            }

            if constexpr (std::is_same_v<T, float>) {
                volk_32fc_magnitude_32f(out, (lv_32fc_t*)in, count);
                dcBlock.process(count, out, out);
                if (_agcMode == AGCMode::AUDIO) {
                    audioAgc.process(count, out, out);
                }
                {
                    std::lock_guard<std::mutex> lck(lpfMtx);
                    lpf.process(count, out, out);
                }
            }
            if constexpr (std::is_same_v<T, stereo_t>) {
                volk_32fc_magnitude_32f(audioAgc.out.writeBuf, (lv_32fc_t*)in, count);
                dcBlock.process(count, audioAgc.out.writeBuf, audioAgc.out.writeBuf);
                if (_agcMode == AGCMode::AUDIO) {
                    audioAgc.process(count, audioAgc.out.writeBuf, audioAgc.out.writeBuf);
                }
                {
                    std::lock_guard<std::mutex> lck(lpfMtx);
                    lpf.process(count, audioAgc.out.writeBuf, audioAgc.out.writeBuf);
                }
                convert::MonoToStereo::process(count, audioAgc.out.writeBuf, out);
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

    protected:
        AGCMode _agcMode;

        double _samplerate;
        double _bandwidth;

        loop::AGC<complex_t> carrierAgc;
        loop::AGC<float> audioAgc;
        correction::DCBlocker<float> dcBlock;
        tap<float> lpfTaps;
        filter::FIR<float, float> lpf;
        std::mutex lpfMtx;

    };
}