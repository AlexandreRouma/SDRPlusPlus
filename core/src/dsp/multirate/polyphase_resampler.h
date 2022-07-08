#pragma once
#include "../processor.h"
#include "../taps/tap.h"
#include "polyphase_bank.h"

namespace dsp::multirate {
    template<class T>
    class PolyphaseResampler : public Processor<T, T> {
        using base_type = Processor<T, T>;
    public:
        PolyphaseResampler() {}

        PolyphaseResampler(stream<T>* in, int interp, int decim, tap<float> taps) { init(in, interp, decim, taps); }

        ~PolyphaseResampler() {
            if (!base_type::_block_init) { return; }
            base_type::stop();
            buffer::free(buffer);
            freePolyphaseBank(phases);
        }

        void init(stream<T>* in, int interp, int decim, tap<float> taps) {
            _interp = interp;
            _decim = decim;
            _taps = taps;

            // Build filter bank
            phases = buildPolyphaseBank(_interp, _taps);

            // Allocate delay buffer
            buffer = buffer::alloc<T>(STREAM_BUFFER_SIZE + 64000);
            bufStart = &buffer[phases.tapsPerPhase - 1];
            buffer::clear<T>(buffer, phases.tapsPerPhase - 1);

            base_type::init(in);
        }

        void setRatio(int interp, int decim, tap<float>& taps) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();

            // Update settings
            _interp = interp;
            _decim = decim;
            _taps = taps;

            // Re-generate polyphase bank
            freePolyphaseBank(phases);
            phases = buildPolyphaseBank(_interp, _taps);

            // Reset buffer
            bufStart = &buffer[phases.tapsPerPhase - 1];
            reset();

            base_type::tempStart();
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            buffer::clear<T>(buffer, phases.tapsPerPhase - 1);
            phase = 0;
            offset = 0;
            base_type::tempStart();
        }

        inline int process(int count, const T* in, T* out) {
            int outCount = 0;

            // Copy input to buffer
            memcpy(bufStart, in, count * sizeof(T));

            while (offset < count) {
                // Do convolution
                if constexpr (std::is_same_v<T, float>) {
                    volk_32f_x2_dot_prod_32f(&out[outCount++], &buffer[offset], phases.phases[phase], phases.tapsPerPhase);
                }
                if constexpr (std::is_same_v<T, complex_t> || std::is_same_v<T, stereo_t>) {
                    volk_32fc_32f_dot_prod_32fc((lv_32fc_t*)&out[outCount++], (lv_32fc_t*)&buffer[offset], phases.phases[phase], phases.tapsPerPhase);
                }

                // Increment phase
                phase += _decim;

                // Branchless phase advance if phase wrap arround occurs
                offset += phase / _interp;

                // Wrap around if needed
                phase = phase % _interp;
            }
            offset -= count;

            // Move delay
            memmove(buffer, &buffer[count], (phases.tapsPerPhase - 1) * sizeof(T));

            return outCount;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            int outCount = process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            // Swap if some data was generated
            base_type::_in->flush();
            if (outCount) {
                if (!base_type::out.swap(outCount)) { return -1; }
            }
            return outCount;
        }

    protected:
        int _interp;
        int _decim;
        tap<float> _taps;
        PolyphaseBank<float> phases;
        int phase = 0;
        int offset = 0;
        T* buffer;
        T* bufStart;

    };
}