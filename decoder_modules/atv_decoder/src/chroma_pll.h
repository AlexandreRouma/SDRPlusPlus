#pragma once
#include <dsp/loop/pll.h>
#include "chrominance_filter.h"

// TODO: Should be 60 but had to try something
#define BURST_START (63+CHROMA_FIR_DELAY)
#define BURST_END   (BURST_START+28)

#define A_PHASE     ((135.0/180.0)*FL_M_PI)
#define B_PHASE     ((-135.0/180.0)*FL_M_PI)

namespace dsp::loop {
    class ChromaPLL : public PLL {
        using base_type = PLL;
    public:
        ChromaPLL() {}

        ChromaPLL(stream<complex_t>* in, double bandwidth, double initPhase = 0.0, double initFreq = 0.0, double minFreq = -FL_M_PI, double maxFreq = FL_M_PI) {
            base_type::init(in, bandwidth, initFreq, initPhase, minFreq, maxFreq);
        }

        inline int process(int count, complex_t* in, complex_t* out, bool aphase = false) {
            // Process the pre-burst section
            for (int i = 0; i < BURST_START; i++) {
                out[i] = in[i] * math::phasor(-pcl.phase);
                pcl.advancePhase();
            }

            // Process the burst itself
            if (aphase) {
                for (int i = BURST_START; i < BURST_END; i++) {
                    complex_t outVal = in[i] * math::phasor(-pcl.phase);
                    out[i] = outVal;
                    pcl.advance(math::normalizePhase(outVal.phase() - A_PHASE));
                }
            }
            else {
                for (int i = BURST_START; i < BURST_END; i++) {
                    complex_t outVal = in[i] * math::phasor(-pcl.phase);
                    out[i] = outVal;
                    pcl.advance(math::normalizePhase(outVal.phase() - B_PHASE));
                }
            }
            
            
            // Process the post-burst section
            for (int i = BURST_END; i < count; i++) {
                out[i] = in[i] * math::phasor(-pcl.phase);
                pcl.advancePhase();
            }

            return count;
        }

        inline int processBlank(int count, complex_t* in, complex_t* out) {
            for (int i = 0; i < count; i++) {
                out[i] = in[i] * math::phasor(-pcl.phase);
                pcl.advancePhase();
            }
            return count;
        }
    };
}