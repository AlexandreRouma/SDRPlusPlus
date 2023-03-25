#pragma once
#include "../processor.h"
#include "quadrature.h"
#include "../filter/fir.h"
#include "../taps/low_pass.h"
#include "../taps/high_pass.h"
#include "../taps/band_pass.h"
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
            dsp::taps::free(filterTaps);
        }

        void init(dsp::stream<dsp::complex_t>* in, double samplerate, double bandwidth, bool lowPass, bool highPass) {
            _samplerate = samplerate;
            _bandwidth = bandwidth;
            _lowPass = lowPass;
            _highPass = highPass;

            demod.init(NULL, bandwidth / 2.0, _samplerate);
            loadDummyTaps();
            fir.init(NULL, filterTaps);

            // Initialize taps
            updateFilter(lowPass, highPass);

            if constexpr (std::is_same_v<T, float>) {
                demod.out.free();
            }
            fir.out.free();

            base_type::init(in);
        }

        void setSamplerate(double samplerate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _samplerate = samplerate;
            demod.setDeviation(_bandwidth / 2.0, _samplerate);
            updateFilter(_lowPass, _highPass);
            base_type::tempStart();
        }

        void setBandwidth(double bandwidth) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            if (bandwidth == _bandwidth) { return; }
            _bandwidth = bandwidth;
            demod.setDeviation(_bandwidth / 2.0, _samplerate);
            updateFilter(_lowPass, _highPass);
        }

        void setLowPass(bool lowPass) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            updateFilter(lowPass, _highPass);
        }

        void setHighPass(bool highPass) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            updateFilter(_lowPass, highPass);
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            demod.reset();
            fir.reset();
            base_type::tempStart();
        }

        inline int process(int count, dsp::complex_t* in, T* out) {
            if constexpr (std::is_same_v<T, float>) {
                demod.process(count, in, out);
                if (filtering) {
                    std::lock_guard<std::mutex> lck(filterMtx);
                    fir.process(count, out, out);
                }
            }
            if constexpr (std::is_same_v<T, stereo_t>) {
                demod.process(count, in, demod.out.writeBuf);
                if (filtering) {
                    std::lock_guard<std::mutex> lck(filterMtx);
                    fir.process(count, demod.out.writeBuf, demod.out.writeBuf);
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
        void updateFilter(bool lowPass, bool highPass) {
            std::lock_guard<std::mutex> lck(filterMtx);

            // Update values
            _lowPass = lowPass;
            _highPass = highPass;
            filtering = (lowPass || highPass);

            // Free filter taps
            dsp::taps::free(filterTaps);

            // Generate filter depending on low and high pass settings
            if (_lowPass && _highPass) {
                filterTaps = dsp::taps::bandPass<float>(300.0, _bandwidth / 2.0, 100.0, _samplerate);
            }
            else if (_highPass) {
                filterTaps = dsp::taps::highPass(300.0, 100.0, _samplerate);
            }
            else if (_lowPass) {
                filterTaps = dsp::taps::lowPass(_bandwidth / 2.0, (_bandwidth / 2.0) * 0.1, _samplerate);
            }
            else {
                loadDummyTaps();
            }

            // Set filter to use new taps
            fir.setTaps(filterTaps);
            fir.reset();
        }

        void loadDummyTaps() {
            float dummyTap = 1.0f;
            filterTaps = dsp::taps::fromArray<float>(1, &dummyTap);
        }

        double _samplerate;
        double _bandwidth;
        bool _lowPass;
        bool _highPass;
        bool filtering;

        Quadrature demod;
        tap<float> filterTaps;
        filter::FIR<float, float> fir;
        std::mutex filterMtx;
    };
}