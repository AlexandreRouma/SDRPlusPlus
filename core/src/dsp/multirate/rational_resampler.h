#pragma once
#include <vector>
#include <numeric>
#include "../processor.h"
#include "../filter/decimating_fir.h"
#include "../taps/from_array.h"
#include "polyphase_resampler.h"
#include "power_decimator.h"
#include "../taps/low_pass.h"
#include "../window/nuttall.h"

namespace dsp::multirate {
    template<class T>
    class RationalResampler : public Processor<T, T> {
        using base_type = Processor<T, T>;
    public:
        RationalResampler() {}

        RationalResampler(stream<T>* in, double inSamplerate, double outSamplerate) { init(in, inSamplerate, outSamplerate); }

        void init(stream<T>* in, double inSamplerate, double outSamplerate) {
            _inSamplerate = inSamplerate;
            _outSamplerate = outSamplerate;
            
            // Dummy initialization since only used for processing
            rtaps = taps::lowPass(0.25, 0.1, 1.0);
            decim.init(NULL, 2);
            resamp.init(NULL, 1, 1, rtaps);

            // Proper configuration
            reconfigure();

            base_type::init(in);
        }

        void reset() {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            decim.reset();
            resamp.reset();
            base_type::tempStart();
        }

        void setInSamplerate(double inSamplerate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _inSamplerate = inSamplerate;
            reconfigure();
            base_type::tempStart();
        }

        void setOutSamplerate(double outSamplerate) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            base_type::tempStop();
            _outSamplerate = outSamplerate;
            reconfigure();
            base_type::tempStart();
        }

        inline int process(int count, const T* in, T* out) {
            switch(mode) {
                case Mode::BOTH:
                    count = decim.process(count, in, out);
                    return resamp.process(count, out, out);
                case Mode::DECIM_ONLY:
                    return decim.process(count, in, out);
                case Mode::RESAMP_ONLY:
                    return resamp.process(count, in, out);
                case Mode::NONE:
                    memcpy(out, in, count * sizeof(T));
                    return count;
            }
            return count;
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
        enum Mode {
            BOTH,
            DECIM_ONLY,
            RESAMP_ONLY,
            NONE
        };

        void reconfigure() {
            // Calculate highest power-of-two decimation for the power decimator 
            int predecPower = std::min<int>(floor(log2(_inSamplerate / _outSamplerate)), PowerDecimator<T>::getMaxRatio());
            int predecRatio = std::min<int>(1 << predecPower, PowerDecimator<T>::getMaxRatio());
            double intSamplerate = _inSamplerate;

            // Configure the DDC
            bool useDecim = (_inSamplerate > _outSamplerate && predecPower > 0);
            if (useDecim) {
                intSamplerate = _inSamplerate / (double)predecRatio;
                decim.setRatio(predecRatio);
            }

            // Calculate interpolation and decimation for polyphase resampler
            int IntSR = round(intSamplerate);
            int OutSR = round(_outSamplerate);
            int gcd = std::gcd(IntSR, OutSR);
            int interp = OutSR / gcd;
            int decim = IntSR / gcd;

            // Check for excessive error
            double actualOutSR = (double)IntSR * (double)interp / (double)decim;
            double error = abs((actualOutSR - _outSamplerate) / _outSamplerate) * 100.0;
            if (error > 0.01) {
                fprintf(stderr, "Warning: resampling error is over 0.01%: %lf\n", error);
            }
            
            // If the power decimator already did all the work, don't use the resampler
            if (interp == decim) {
                mode = useDecim ? Mode::DECIM_ONLY : Mode::NONE;
                return;
            }

            // Configure the polyphase resampler
            double tapSamplerate = intSamplerate * (double)interp;
            double tapBandwidth = std::min<double>(_inSamplerate, _outSamplerate) / 2.0;
            double tapTransWidth = tapBandwidth * 0.1;
            taps::free(rtaps);
            rtaps = taps::lowPass(tapBandwidth, tapTransWidth, tapSamplerate);
            for (int i = 0; i < rtaps.size; i++) { rtaps.taps[i] *= (float)interp; }
            resamp.setRatio(interp, decim, rtaps);

            printf("[Resamp] predec: %d, interp: %d, decim: %d, inacc: %lf%%, taps: %d\n", predecRatio, interp, decim, error, rtaps.size);

            mode = useDecim ? Mode::BOTH : Mode::RESAMP_ONLY;
        }
        
        PowerDecimator<T> decim;
        PolyphaseResampler<T> resamp;
        tap<float> rtaps;
        double _inSamplerate;
        double _outSamplerate;
        Mode mode;
    };
}