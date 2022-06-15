#pragma once
#include <math.h>
#include <assert.h>
#include "../types.h"

namespace dsp::loop {
    template<class T>
    class PhaseControlLoop {
    public:
        PhaseControlLoop() {}

        PhaseControlLoop(T alpha, T beta, T phase, T minPhase, T maxPhase, T freq, T minFreq, T maxFreq) {
            init(alpha, beta, phase, minPhase, maxPhase, freq, minFreq, maxFreq);
        }

        void init(T alpha, T beta, T phase, T minPhase, T maxPhase, T freq, T minFreq, T maxFreq) {
            assert(maxPhase > minPhase);
            assert(maxFreq > minFreq);
            _alpha = alpha;
            _beta = beta;
            this->phase = phase;
            _minPhase = minPhase;
            _maxPhase = maxPhase;
            this->freq = freq;
            _minFreq = minFreq;
            _maxFreq = maxFreq;

            phaseDelta = _maxPhase - _minPhase;
        }

        static inline void criticallyDamped(T bandwidth, T& alpha, T& beta) {
            T dampningFactor = sqrt(2.0) / 2.0;
            T denominator = (1.0 + 2.0*dampningFactor*bandwidth + bandwidth*bandwidth);
            alpha = (4 * dampningFactor * bandwidth) / denominator;
            beta = (4 * bandwidth * bandwidth) / denominator;
        }

        void setPhaseLimits(T minPhase, T maxPhase) {
            assert(maxPhase > minPhase);
            _minPhase = minPhase;
            _maxPhase = maxPhase;
            phaseDelta = _maxPhase - _minPhase;
            clampPhase();
        }

        void setFreqLimits(T minFreq, T maxFreq) {
            assert(maxFreq > minFreq);
            _minFreq = minFreq;
            _maxFreq = maxFreq;
            clampFreq();
        }

        inline void advance(T error) {
            // Increment and clamp frequency
            freq += _beta * error;
            clampFreq();

            // Increment and clamp phase
            phase += freq + (_alpha * error);
            clampPhase();
        }

        T freq;
        T phase;

    protected:
        inline void clampFreq() {
            if (freq > _maxFreq) { freq = _maxFreq; }
            else if (freq < _minFreq) { freq = _minFreq; }
        }

        inline void clampPhase() {
            while (phase > _maxPhase) { phase -= phaseDelta; }
            while (phase < _minPhase) { phase += phaseDelta; }
        }

        T _alpha;
        T _beta;
        T _minPhase;
        T _maxPhase;
        T _minFreq;
        T _maxFreq;
        T phaseDelta;
    };
}