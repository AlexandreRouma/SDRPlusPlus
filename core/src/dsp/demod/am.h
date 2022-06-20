#pragma once
#include "../processor.h"
#include "../loop/agc.h"
#include "../correction/dc_blocker.h"

namespace dsp::demod {
    class AM : public Processor<dsp::complex_t, float> {
        using base_type = Processor<dsp::complex_t, float>;
    public:
        enum AGCMode {
            CARRIER,
            AUDIO,
        };

        AM() {}

        AM(stream<complex_t>* in, AGCMode agcMode, double agcAttack, double agcDecay, double dcBlockRate) { init(in, agcMode, agcAttack, agcDecay, dcBlockRate); }

        void init(stream<complex_t>* in, AGCMode agcMode, double agcAttack, double agcDecay, double dcBlockRate) {
            _agcMode = agcMode;

            carrierAgc.init(NULL, 1.0, agcAttack, agcDecay, 10e6, 10.0);
            audioAgc.init(NULL, 1.0, agcAttack, agcDecay, 10e6, 10.0);
            dcBlock.init(NULL, dcBlockRate);

            audioAgc.out.free();
            dcBlock.out.free();
            
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

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            carrierAgc.reset();
            audioAgc.reset();
            dcBlock.reset();
            base_type::tempStart();
        }

        int process(int count, complex_t* in, float* out) {
            // Apply carrier AGC if needed
            if (_agcMode == AGCMode::CARRIER) {
                carrierAgc.process(count, in, carrierAgc.out.writeBuf);
                in = carrierAgc.out.writeBuf;
            }

            // Get magnitude of each sample and remove DC (TODO: use block instead)
            volk_32fc_magnitude_32f(out, (lv_32fc_t*)in, count);
            dcBlock.process(count, out, out);

            // Apply audio AGC if needed
            if (_agcMode == AGCMode::AUDIO) {
                audioAgc.process(count, out, out);
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

        loop::AGC<complex_t> carrierAgc;
        loop::AGC<float> audioAgc;
        correction::DCBlocker<float> dcBlock;

    };
}