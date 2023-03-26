#pragma once
#include "../processor.h"
#include "../window/nuttall.h"
#include <fftw3.h>
#include "../convert/stereo_to_mono.h"

namespace dsp::noise_reduction {
    class Audio : public Processor<stereo_t, stereo_t> {
        using base_type = Processor<stereo_t, stereo_t>;
    public:
        Audio() {}

        Audio(stream<stereo_t>* in, int bins) { init(in, bins); }

        ~Audio() {
            if (!base_type::_block_init) { return; }
            base_type::stop();
            destroyBuffers();
        }

        void init(stream<stereo_t>* in, int bins) {
            _bins = bins;
            complexBins = (bins / 2) + 1;
            normFactor = 1.0f / (float)_bins;
            initBuffers();
            base_type::init(in);
        }

        void setBins(int bins) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _bins = bins;
            complexBins = (bins / 2) + 1;
            normFactor = 1.0f / (float)_bins;
            destroyBuffers();
            initBuffers();
            base_type::tempStart();
        }

        void setLevel(float level) {
            _level = powf(10.0f, level * 0.1f);
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            buffer::clear(buffer, _bins - 1);
            buffer::clear(backFFTIn, _bins);
            buffer::clear(noisePrint, _bins);
            base_type::tempStart();
        }

        int process(int count, const stereo_t* in, stereo_t* out) {
            // Write new input data to buffer
            convert::StereoToMono::process(count, in, bufferStart);
            
            // Iterate the FFT
            for (int i = 0; i < count; i++) {
                // Apply windows
                volk_32f_x2_multiply_32f(forwFFTIn, &buffer[i], fftWin, _bins);

                // Do forward FFT
                fftwf_execute(forwardPlan);

                // Get bin amplitude and square to get power
                volk_32fc_magnitude_32f(ampBuf, (lv_32fc_t*)forwFFTOut, complexBins);

                // Update noise print using a running average
                volk_32f_s32f_multiply_32f(scaledAmps, ampBuf, alpha, complexBins);
                volk_32f_s32f_multiply_32f(noisePrint, noisePrint, beta, complexBins);
                volk_32f_x2_add_32f(noisePrint, noisePrint, scaledAmps, complexBins);

                // Clamp amplitudes
                volk_32f_x2_max_32f(ampBuf, ampBuf, noisePrint, complexBins);

                // Compute Wiener (funny) filter
                volk_32f_x2_subtract_32f(scaledAmps, ampBuf, noisePrint, complexBins);
                volk_32f_x2_divide_32f(scaledAmps, scaledAmps, ampBuf, complexBins);

                // Apply wiener filter to bins
                volk_32fc_32f_multiply_32fc((lv_32fc_t*)backFFTIn, (lv_32fc_t*)forwFFTOut, scaledAmps, complexBins);

                // Do reverse FFT and get first element
                fftwf_execute(backwardPlan);
                out[i].l = backFFTOut[_bins / 2];
                out[i].r = backFFTOut[_bins / 2];
            }

            // Correct amplitude
            volk_32f_s32f_multiply_32f((float*)out, (float*)out, normFactor, count*2);

            // Move buffer buffer
            memmove(buffer, &buffer[count], (_bins - 1) * sizeof(float));

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
            forwFFTIn = (float*)fftwf_malloc(_bins * sizeof(float));
            forwFFTOut = (complex_t*)fftwf_malloc(_bins * sizeof(complex_t));
            backFFTIn = (complex_t*)fftwf_malloc(_bins * sizeof(complex_t));
            backFFTOut = (float*)fftwf_malloc(_bins * sizeof(float));

            // Allocate and clear delay buffer
            buffer = buffer::alloc<float>(STREAM_BUFFER_SIZE + 64000);
            bufferStart = &buffer[_bins - 1];
            buffer::clear(buffer, _bins - 1);

            // Clear backward FFT input
            buffer::clear(backFFTIn, _bins);

            // Allocate amplitude buffer
            ampBuf = buffer::alloc<float>(_bins);
            scaledAmps = buffer::alloc<float>(_bins);
            noisePrint = buffer::alloc<float>(_bins);
            buffer::clear(noisePrint, _bins);

            // Allocate and generate Window
            fftWin = buffer::alloc<float>(_bins);
            for (int i = 0; i < _bins; i++) { fftWin[i] = window::nuttall(i, _bins - 1); }

            // Plan FFTs
            forwardPlan = fftwf_plan_dft_r2c_1d(_bins, forwFFTIn, (fftwf_complex*)forwFFTOut, FFTW_ESTIMATE);
            backwardPlan = fftwf_plan_dft_c2r_1d(_bins, (fftwf_complex*)backFFTIn, backFFTOut, FFTW_ESTIMATE);
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
            buffer::free(scaledAmps);
            buffer::free(noisePrint);
            buffer::free(fftWin);
        }

        float _level = 0.0f;

        float* forwFFTIn;
        complex_t* forwFFTOut;
        complex_t* backFFTIn;
        float* backFFTOut;

        fftwf_plan forwardPlan;
        fftwf_plan backwardPlan;

        float* buffer;
        float* bufferStart;

        float* fftWin;

        float* ampBuf;
        float* scaledAmps;
        float* noisePrint;

        int _bins;
        int complexBins;
        float normFactor = 1.0f;

        float alpha = 0.0001f;
        float beta = 0.9999f;
    }; 
}