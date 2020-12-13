#pragma once
#include <dsp/block.h>

namespace dsp {
    class MonoToStereo : public generic_block<MonoToStereo> {
    public:
        MonoToStereo() {}

        MonoToStereo(stream<float>* in) { init(in); }

        ~MonoToStereo() { generic_block<MonoToStereo>::stop(); }

        void init(stream<float>* in) {
            _in = in;
            generic_block<MonoToStereo>::registerInput(_in);
            generic_block<MonoToStereo>::registerOutput(&out);
        }

        void setInput(stream<float>* in) {
            std::lock_guard<std::mutex> lck(generic_block<MonoToStereo>::ctrlMtx);
            generic_block<MonoToStereo>::tempStop();
            generic_block<MonoToStereo>::unregisterInput(_in);
            _in = in;
            generic_block<MonoToStereo>::registerInput(_in);
            generic_block<MonoToStereo>::tempStart();
        }

        int run() {
            count = _in->read();
            if (count < 0) { return -1; }

            if (out.aquire() < 0) { return -1; }
            for (int i = 0; i < count; i++) {
                out.data[i].l = _in->data[i];
                out.data[i].r = _in->data[i];
            }

            _in->flush();
            out.write(count);
            return count;
        }

        stream<stereo_t> out;

    private:
        int count;
        stream<float>* _in;

    };

    class StereoToMono : public generic_block<StereoToMono> {
    public:
        StereoToMono() {}

        StereoToMono(stream<stereo_t>* in) { init(in); }

        ~StereoToMono() { generic_block<StereoToMono>::stop(); }

        void init(stream<stereo_t>* in) {
            _in = in;
            generic_block<StereoToMono>::registerInput(_in);
            generic_block<StereoToMono>::registerOutput(&out);
        }

        void setInput(stream<stereo_t>* in) {
            std::lock_guard<std::mutex> lck(generic_block<StereoToMono>::ctrlMtx);
            generic_block<StereoToMono>::tempStop();
            generic_block<StereoToMono>::unregisterInput(_in);
            _in = in;
            generic_block<StereoToMono>::registerInput(_in);
            generic_block<StereoToMono>::tempStart();
        }

        int run() {
            count = _in->read();
            if (count < 0) { return -1; }

            if (out.aquire() < 0) { return -1; }
            for (int i = 0; i < count; i++) {
                out.data[i] = (_in->data[i].l + _in->data[i].r) / 2.0f;
            }

            _in->flush();
            out.write(count);
            return count;
        }

        stream<float> out;

    private:
        int count;
        stream<stereo_t>* _in;

    };
}