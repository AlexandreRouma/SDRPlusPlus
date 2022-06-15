#include "../block.h"


namespace dsp::buffer {
    template <class T>
    class Packer : public block {
    public:
        Packer() {}

        Packer(stream<T>* in, int count) { init(in, count); }

        void init(stream<T>* in, int count) {
            _in = in;
            samples = count;
            block::registerInput(_in);
            block::registerOutput(&out);
            block::_block_init = true;
        }

        void setInput(stream<T>* in) {
            assert(block::_block_init);
            std::lock_guard<std::recursive_mutex> lck(block::ctrlMtx);
            block::tempStop();
            block::unregisterInput(_in);
            _in = in;
            block::registerInput(_in);
            block::tempStart();
        }

        void setSampleCount(int count) {
            assert(block::_block_init);
            std::lock_guard<std::recursive_mutex> lck(block::ctrlMtx);
            block::tempStop();
            samples = count;
            block::tempStart();
        }

        int run() {
            int count = _in->read();
            if (count < 0) {
                read = 0;
                return -1;
            }

            for (int i = 0; i < count; i++) {
                out.writeBuf[read++] = _in->readBuf[i];
                if (read >= samples) {
                    read = 0;
                    if (!out.swap(samples)) {
                        _in->flush();
                        read = 0;
                        return -1;
                    }
                }
            }

            _in->flush();

            return count;
        }

        stream<T> out;

    private:
        int samples = 1;
        int read = 0;
        stream<T>* _in;
    };
}