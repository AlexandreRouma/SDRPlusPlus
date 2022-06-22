#pragma once
#include "../sink.h"

namespace dsp::routing {
    template <class T>
    class Splitter : public Sink<T> {
        using base_type = Sink<T>;
    public:
        Splitter() {}

        Splitter(stream<T>* in) { base_type::init(in); }

        void bindStream(stream<T>* stream) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            
            // Check that the stream isn't already bound
            if (std::find(streams.begin(), streams.end(), stream) != streams.end()) {
                throw std::runtime_error("[Splitter] Tried to bind stream to that is already bound");
            }

            // Add to the list
            base_type::tempStop();
            streams.push_back(stream);
            base_type::registerOutput(stream);
            base_type::tempStart();
        }

        void unbindStream(stream<T>* stream) {
            assert(base_type::_block_init);
            std::lock_guard<std::recursive_mutex> lck(base_type::ctrlMtx);
            
            // Check that the stream is bound
            auto sit = std::find(streams.begin(), streams.end(), stream);
            if (sit == streams.end()) {
                throw std::runtime_error("[Splitter] Tried to unbind stream to that isn't bound");
            }

            // Add to the list
            base_type::tempStop();
            base_type::unregisterOutput(stream);
            streams.erase(sit);
            base_type::tempStart();
        }

        int run() {
            int count = base_type::_in->read();
            if (count < 0) { return -1; }

            for (const auto& stream : streams) {
                memcpy(stream->writeBuf, base_type::_in->readBuf, count * sizeof(T));
                if (!stream->swap(count)) {
                    base_type::_in->flush();
                    return -1;
                }
            }

            base_type::_in->flush();

            return count;
        }

    protected:
        std::vector<stream<T>*> streams;

    };
}