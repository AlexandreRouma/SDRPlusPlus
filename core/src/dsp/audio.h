#pragma once
#include <dsp/block.h>

namespace dsp {
    class MonoToStereo : public generic_block<MonoToStereo> {
    public:
        MonoToStereo() {}

        MonoToStereo(stream<float>* in) { init(in); }

        void init(stream<float>* in) {
            _in = in;
            generic_block<MonoToStereo>::registerInput(_in);
            generic_block<MonoToStereo>::registerOutput(&out);
            generic_block<MonoToStereo>::_block_init = true;
        }

        void setInput(stream<float>* in) {
            assert(generic_block<MonoToStereo>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<MonoToStereo>::ctrlMtx);
            generic_block<MonoToStereo>::tempStop();
            generic_block<MonoToStereo>::unregisterInput(_in);
            _in = in;
            generic_block<MonoToStereo>::registerInput(_in);
            generic_block<MonoToStereo>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            volk_32f_x2_interleave_32fc((lv_32fc_t*)out.writeBuf, _in->readBuf, _in->readBuf, count);

            _in->flush();
            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<stereo_t> out;

    private:
        stream<float>* _in;

    };

    class ChannelsToStereo : public generic_block<ChannelsToStereo> {
    public:
        ChannelsToStereo() {}

        ChannelsToStereo(stream<float>* in_left, stream<float>* in_right) { init(in_left, in_right); }

        void init(stream<float>* in_left, stream<float>* in_right) {
            _in_left = in_left;
            _in_right = in_right;
            generic_block<ChannelsToStereo>::registerInput(_in_left);
            generic_block<ChannelsToStereo>::registerInput(_in_right);
            generic_block<ChannelsToStereo>::registerOutput(&out);
            generic_block<ChannelsToStereo>::_block_init = true;
        }

        void setInput(stream<float>* in_left, stream<float>* in_right) {
            assert(generic_block<ChannelsToStereo>::_block_init);
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
            int count_l = _in_left->read();
            if (count_l < 0) { return -1; }
            int count_r = _in_right->read();
            if (count_r < 0) { return -1; }

            if (count_l != count_r) {
                spdlog::warn("ChannelsToStereo block size missmatch");
            }

            volk_32f_x2_interleave_32fc((lv_32fc_t*)out.writeBuf, _in_left->readBuf, _in_right->readBuf, count_l);

            _in_left->flush();
            _in_right->flush();
            if (!out.swap(count_l)) { return -1; }
            return count_l;
        }

        stream<stereo_t> out;

    private:
        stream<float>* _in_left;
        stream<float>* _in_right;

    };

    class StereoToMono : public generic_block<StereoToMono> {
    public:
        StereoToMono() {}

        StereoToMono(stream<stereo_t>* in) { init(in); }

        ~StereoToMono() {
            if (!generic_block<StereoToMono>::_block_init) { return; }
            generic_block<StereoToMono>::stop();
            delete[] l_buf;
            delete[] r_buf;
            generic_block<StereoToMono>::_block_init = false;
        }

        void init(stream<stereo_t>* in) {
            _in = in;
            l_buf = new float[STREAM_BUFFER_SIZE];
            r_buf = new float[STREAM_BUFFER_SIZE];
            generic_block<StereoToMono>::registerInput(_in);
            generic_block<StereoToMono>::registerOutput(&out);
            generic_block<StereoToMono>::_block_init = true;
        }

        void setInput(stream<stereo_t>* in) {
            assert(generic_block<StereoToMono>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<StereoToMono>::ctrlMtx);
            generic_block<StereoToMono>::tempStop();
            generic_block<StereoToMono>::unregisterInput(_in);
            _in = in;
            generic_block<StereoToMono>::registerInput(_in);
            generic_block<StereoToMono>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            for (int i = 0; i < count; i++) {
                out.writeBuf[i] = (_in->readBuf[i].l + _in->readBuf[i].r) * 0.5f;
            }

            _in->flush();

            if (!out.swap(count)) { return -1; }
            return count;
        }

        stream<float> out;

    private:
        float* l_buf, *r_buf;
        stream<stereo_t>* _in;

    };

    class StereoToChannels : public generic_block<StereoToChannels> {
    public:
        StereoToChannels() {}

        StereoToChannels(stream<stereo_t>* in) { init(in); }

        void init(stream<stereo_t>* in) {
            _in = in;
            generic_block<StereoToChannels>::registerInput(_in);
            generic_block<StereoToChannels>::registerOutput(&out_left);
            generic_block<StereoToChannels>::registerOutput(&out_right);
            generic_block<StereoToChannels>::_block_init = true;
        }

        void setInput(stream<stereo_t>* in) {
            assert(generic_block<StereoToChannels>::_block_init);
            std::lock_guard<std::mutex> lck(generic_block<StereoToChannels>::ctrlMtx);
            generic_block<StereoToChannels>::tempStop();
            generic_block<StereoToChannels>::unregisterInput(_in);
            _in = in;
            generic_block<StereoToChannels>::registerInput(_in);
            generic_block<StereoToChannels>::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) { return -1; }

            volk_32fc_deinterleave_32f_x2(out_left.writeBuf, out_right.writeBuf, (lv_32fc_t*)_in->readBuf, count);

            _in->flush();
            if (!out_left.swap(count)) { return -1; }
            if (!out_right.swap(count)) { return -1; }
            return count;
        }

        stream<float> out_left;
        stream<float> out_right;

    private:
        stream<stereo_t>* _in;

    };
}