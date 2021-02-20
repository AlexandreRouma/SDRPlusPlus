#pragma once
#include <dsp/block.h>
#include <dsp/types.h>

namespace dsp {
    namespace filter_window {
        class generic_window {
        public:
            virtual int getTapCount() { return -1; }
            virtual void createTaps(float* taps, int tapCount, float factor = 1.0f) {}
        };

        class generic_complex_window {
        public:
            virtual int getTapCount() { return -1; }
            virtual void createTaps(dsp::complex_t* taps, int tapCount, float factor = 1.0f) {}
        };

        class BlackmanWindow : public filter_window::generic_window {
        public:
            BlackmanWindow() {}
            BlackmanWindow(float cutoff, float transWidth, float sampleRate) { init(cutoff, transWidth, sampleRate); }

            void init(float cutoff, float transWidth, float sampleRate) {
                _cutoff = cutoff;
                _transWidth = transWidth;
                _sampleRate = sampleRate;
            }

            void setSampleRate(float sampleRate) {
                _sampleRate = sampleRate;
            }

            void setCutoff(float cutoff) {
                _cutoff = cutoff;
            }
            
            void setTransWidth(float transWidth) {
                _transWidth = transWidth;
            }

            int getTapCount() {
                float fc = _cutoff / _sampleRate;
                if (fc > 1.0f) {
                    fc = 1.0f;
                }

                int _M = 4.0f / (_transWidth / _sampleRate);
                if (_M < 4) {
                    _M = 4;
                }

                if (_M % 2 == 0) { _M++; }

                return _M;
            }

            void createTaps(float* taps, int tapCount, float factor = 1.0f) {
                float fc = _cutoff / _sampleRate;
                if (fc > 1.0f) {
                    fc = 1.0f;
                }
                float tc = tapCount;
                float sum = 0.0f;
                float val;
                for (int i = 0; i < tapCount; i++) {
                    val = (sin(2.0f * FL_M_PI * fc * ((float)i - (tc / 2))) / ((float)i - (tc / 2))) * 
                        (0.42f - (0.5f * cos(2.0f * FL_M_PI / tc)) + (0.8f * cos(4.0f * FL_M_PI / tc)));
                    taps[i] = val; // tapCount - i - 1
                    sum += val;
                }
                for (int i = 0; i < tapCount; i++) {
                    taps[i] *= factor;
                    taps[i] /= sum;
                }
            }

        private:
            float _cutoff, _transWidth, _sampleRate;

        };

        class BlackmanBandpassWindow : public filter_window::generic_window {
        public:
            BlackmanBandpassWindow() {}
            BlackmanBandpassWindow(float cutoff, float transWidth, float offset, float sampleRate) { init(cutoff, transWidth, offset, sampleRate); }

            void init(float cutoff, float transWidth, float offset, float sampleRate) {
                _cutoff = cutoff;
                _transWidth = transWidth;
                _offset = offset;
                _sampleRate = sampleRate;
            }

            void setSampleRate(float sampleRate) {
                _sampleRate = sampleRate;
            }

            void setCutoff(float cutoff) {
                _cutoff = cutoff;
            }
            
            void setTransWidth(float transWidth) {
                _transWidth = transWidth;
            }

            void setOffset(float offset) {
                _offset = offset;
            }

            int getTapCount() {
                float fc = _cutoff / _sampleRate;
                if (fc > 1.0f) {
                    fc = 1.0f;
                }

                int _M = 4.0f / (_transWidth / _sampleRate);
                if (_M < 4) {
                    _M = 4;
                }

                if (_M % 2 == 0) { _M++; }

                return _M;
            }

            void createTaps(float* taps, int tapCount, float factor = 1.0f) {
                float fc = _cutoff / _sampleRate;
                if (fc > 1.0f) {
                    fc = 1.0f;
                }
                float tc = tapCount;
                float sum = 0.0f;
                float val;
                for (int i = 0; i < tapCount; i++) {
                    val = (sin(2.0f * FL_M_PI * fc * ((float)i - (tc / 2))) / ((float)i - (tc / 2))) * 
                        (0.42f - (0.5f * cos(2.0f * FL_M_PI / tc)) + (0.8f * cos(4.0f * FL_M_PI / tc)));
                    taps[i] = val; // tapCount - i - 1
                    sum += val;
                }
                for (int i = 0; i < tapCount; i++) {
                    taps[i] *= cos(2.0f * (_offset / _sampleRate) * FL_M_PI * (float)i);
                    taps[i] *= factor;
                    taps[i] /= sum;
                }
            }

        private:
            float _cutoff, _transWidth, _sampleRate, _offset;

        };

        class BlackmanBandpassWindow : public filter_window::generic_complex_window {
        public:
            BlackmanBandpassWindow() {}
            BlackmanBandpassWindow(float cutoff, float transWidth, float offset, float sampleRate) { init(cutoff, transWidth, offset, sampleRate); }

            void init(float cutoff, float transWidth, float offset, float sampleRate) {
                _cutoff = cutoff;
                _transWidth = transWidth;
                _offset = offset;
                _sampleRate = sampleRate;
            }

            void setSampleRate(float sampleRate) {
                _sampleRate = sampleRate;
            }

            void setCutoff(float cutoff) {
                _cutoff = cutoff;
            }
            
            void setTransWidth(float transWidth) {
                _transWidth = transWidth;
            }

            void setOffset(float offset) {
                _offset = offset;
            }

            int getTapCount() {
                float fc = _cutoff / _sampleRate;
                if (fc > 1.0f) {
                    fc = 1.0f;
                }

                int _M = 4.0f / (_transWidth / _sampleRate);
                if (_M < 4) {
                    _M = 4;
                }

                if (_M % 2 == 0) { _M++; }

                return _M;
            }

            void createTaps(dsp::complex_t* taps, int tapCount, float factor = 1.0f) {
                float fc = _cutoff / _sampleRate;
                if (fc > 1.0f) {
                    fc = 1.0f;
                }
                float tc = tapCount;
                float sum = 0.0f;
                float val;
                for (int i = 0; i < tapCount; i++) {
                    val = (sin(2.0f * FL_M_PI * fc * ((float)i - (tc / 2))) / ((float)i - (tc / 2))) * 
                        (0.42f - (0.5f * cos(2.0f * FL_M_PI / tc)) + (0.8f * cos(4.0f * FL_M_PI / tc)));
                    taps[i] = val; // tapCount - i - 1
                    sum += val;
                }
                for (int i = 0; i < tapCount; i++) {
                    taps[i] *= cos(2.0f * (_offset / _sampleRate) * FL_M_PI * (float)i);
                    taps[i] *= factor;
                    taps[i] /= sum;
                }
            }

        private:
            float _cutoff, _transWidth, _sampleRate, _offset;

        };
    }
}