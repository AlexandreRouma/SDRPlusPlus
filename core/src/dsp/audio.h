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

            for (int i = 0; i < count; i++) {
                out.writeBuf[i].l = _in->readBuf[i];
                out.writeBuf[i].r = _in->readBuf[i];
            }

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<stereo_t> out;

    private:
        int count;
        stream<float>* _in;

    };

    class ChannelsToStereo : public generic_block<ChannelsToStereo> {
    public:
        ChannelsToStereo() {}

        ChannelsToStereo(stream<float>* in_left, stream<float>* in_right) { init(in_left, in_right); }

        ~ChannelsToStereo() { generic_block<ChannelsToStereo>::stop(); }

        void init(stream<float>* in_left, stream<float>* in_right) {
            _in_left = in_left;
            _in_right = in_right;
            generic_block<ChannelsToStereo>::registerInput(_in_left);
            generic_block<ChannelsToStereo>::registerInput(_in_right);
            generic_block<ChannelsToStereo>::registerOutput(&out);
        }

        void setInput(stream<float>* in_left, stream<float>* in_right) {
            std::lock_guard<std::mutex> lck(generic_block<ChannelsToStereo>::ctrlMtx);
            generic_block<ChannelsToStereo>::tempStop();
            generic_block<ChannelsToStereo>::unregisterInput(_in_left);
            generic_block<ChannelsToStereo>::unregisterInput(_in_right);
            _in_left = in_left;
            _in_right = in_right;
            generic_block<ChannelsToStereo>::registerInput(_in_left);
            generic_block<ChannelsToStereo>::registerInput(_in_right);
            generic_block<ChannelsToStereo>::tempStart();
        }

        int run() {
            count_l = _in_left->read();
            if (count_l < 0) { return -1; }
            count_r = _in_right->read();
            if (count_r < 0) { return -1; }

            if (count_l != count_r) {
                spdlog::warn("ChannelsToStereo block size missmatch");
            }

            for (int i = 0; i < count_l; i++) {
                out.writeBuf[i].l = _in_left->readBuf[i];
                out.writeBuf[i].r = _in_right->readBuf[i];
            }

            _in_left->flush();
            _in_right->flush();
            if (!out.swap(count_l)) { return -1; }
            return count_l;
        }

        stream<stereo_t> out;

    private:
        int count_l;
        int count_r;
        stream<float>* _in_left;
        stream<float>* _in_right;

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

            for (int i = 0; i < count; i++) {
                out.writeBuf[i] = (_in->readBuf[i].l + _in->readBuf[i].r) / 2.0f;
            }

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<float> out;

    private:
        int count;
        stream<stereo_t>* _in;

    };

    class StereoToChannels : public generic_block<StereoToChannels> {
    public:
        StereoToChannels() {}

        StereoToChannels(stream<stereo_t>* in) { init(in); }

        ~StereoToChannels() { generic_block<StereoToChannels>::stop(); }

        void init(stream<stereo_t>* in) {
            _in = in;
            generic_block<StereoToChannels>::registerInput(_in);
            generic_block<StereoToChannels>::registerOutput(&out_left);
            generic_block<StereoToChannels>::registerOutput(&out_right);
        }

        void setInput(stream<stereo_t>* in) {
            std::lock_guard<std::mutex> lck(generic_block<StereoToChannels>::ctrlMtx);
            generic_block<StereoToChannels>::tempStop();
            generic_block<StereoToChannels>::unregisterInput(_in);
            _in = in;
            generic_block<StereoToChannels>::registerInput(_in);
            generic_block<StereoToChannels>::tempStart();
        }

        int run() {
            count = _in->read();
            if (count < 0) { return -1; }

            for (int i = 0; i < count; i++) {
                out_left.writeBuf[i] = _in->readBuf[i].l;
                out_right.writeBuf[i] = _in->readBuf[i].r;
            }

            _in->flush();
            if (!out_left.swap(count)) { return -1; }
            if (!out_right.swap(count)) { return -1; }
            return count;
        }

        stream<float> out_left;
        stream<float> out_right;

    private:
        int count;
        stream<stereo_t>* _in;

    };
}