#pragma once
#include "../processor.h"
#include "../window/nuttall.h"
#include <fftw3.h>

namespace dsp::noise_reduction {
    class FMIF : public Processor<complex_t, complex_t> {
        using base_type = Processor<complex_t, complex_t>;
    public:
        FMIF() {}

        FMIF(stream<complex_t>* in, int bins) { init(in, bins); }

        ~FMIF() {
            if (!base_type::_block_init) { return; }
            base_type::stop();
            destroyBuffers();
        }

        void init(stream<complex_t>* in, int bins) {
            _bins = bins;
            initBuffers();
            base_type::init(in);
        }

        void setBins(int bins) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _bins = bins;
            destroyBuffers();
            initBuffers();
            base_type::tempStart();
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            buffer::clear(buffer, _bins - 1);
            buffer::clear(backFFTIn, _bins);
            base_type::tempStart();
        }

        int process(int count, const complex_t* in, complex_t* out) {
            // Write new input data to buffer buffer
            memcpy(bufferStart, in, count * sizeof(complex_t));
            
            // Iterate the FFT
            for (int i = 0; i < count; i++) {
                // Apply windows
                volk_32fc_32f_multiply_32fc((lv_32fc_t*)forwFFTIn, (lv_32fc_t*)&buffer[i], fftWin, _bins);

                // Do forward FFT
                fftwf_execute(forwardPlan);

                // Process bins here
                uint32_t idx;
                volk_32fc_magnitude_32f(ampBuf, (lv_32fc_t*)forwFFTOut, _bins);
                volk_32f_index_max_32u(&idx, ampBuf, _bins);

                // Keep only the bin of highest amplitude
                backFFTIn[idx] = forwFFTOut[idx];

                // Do reverse FFT and get first element
                fftwf_execute(backwardPlan);
                out[i] = backFFTOut[_bins / 2];

                // Reset the input buffer
                backFFTIn[idx] = { 0, 0 };
            }

            // Move buffer buffer
            memmove(buffer, &buffer[count], (_bins - 1) * sizeof(complex_t));

            return count;
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            process(count, base_type::_in->readBuf, base_type::out.writeBuf);

            // Swap if some data was generated
            base_type::_in->flush();
            if (!base_type::out.swap(count)) { return -1; }
            return count;
        }

    protected:
        void initBuffers() {
            // Allocate FFT buffers
            forwFFTIn = (complex_t*)fftwf_malloc(_bins * sizeof(complex_t));
            forwFFTOut = (complex_t*)fftwf_malloc(_bins * sizeof(complex_t));
            backFFTIn = (complex_t*)fftwf_malloc(_bins * sizeof(complex_t));
            backFFTOut = (complex_t*)fftwf_malloc(_bins * sizeof(complex_t));

            // Allocate and clear delay buffer
            buffer = buffer::alloc<complex_t>(STREAM_BUFFER_SIZE + 64000);
            bufferStart = &buffer[_bins - 1];
            buffer::clear(buffer, _bins - 1);

            // Clear backward FFT input since only one value is changed and reset at a time
            buffer::clear(backFFTIn, _bins);

            // Allocate amplitude buffer
            ampBuf = buffer::alloc<float>(_bins);

            // Allocate and generate Window
            fftWin = buffer::alloc<float>(_bins);
            for (int i = 0; i < _bins; i++) { fftWin[i] = window::nuttall(i, _bins - 1); }

            // Plan FFTs
            forwardPlan = fftwf_plan_dft_1d(_bins, (fftwf_complex*)forwFFTIn, (fftwf_complex*)forwFFTOut, FFTW_FORWARD, FFTW_ESTIMATE);
            backwardPlan = fftwf_plan_dft_1d(_bins, (fftwf_complex*)backFFTIn, (fftwf_complex*)backFFTOut, FFTW_BACKWARD, FFTW_ESTIMATE);
        }

        void destroyBuffers() {
            fftwf_destroy_plan(forwardPlan);
            fftwf_destroy_plan(backwardPlan);
            fftwf_free(forwFFTIn);
            fftwf_free(forwFFTOut);
            fftwf_free(backFFTIn);
            fftwf_free(backFFTOut);
            buffer::free(buffer);
            buffer::free(ampBuf);
            buffer::free(fftWin);
        }

        complex_t* forwFFTIn;
        complex_t* forwFFTOut;
        complex_t* backFFTIn;
        complex_t* backFFTOut;

        fftwf_plan forwardPlan;
        fftwf_plan backwardPlan;

        complex_t* buffer;
        complex_t* bufferStart;

        float* fftWin;

        float* ampBuf;

        int _bins;

    }; 
}