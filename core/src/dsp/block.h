#pragma once
#include <assert.h>
#include <thread>
#include <vector>
#include <algorithm>
#include "stream.h"
#include "types.h"

namespace dsp {
    class generic_block {
    public:
        virtual void start() {}
        virtual void stop() {}
        virtual int run() { return -1; }
    };

    class block : public generic_block {
    public:
        virtual void init() {}

        virtual ~block() {
            if (!_block_init) { return; }
            stop();
            _block_init = false;
        }

        virtual void start() {
            assert(_block_init);
            std::lock_guard<std::recursive_mutex> lck(ctrlMtx);
            if (running) {
                return;
            }
            running = true;
            doStart();
        }

        virtual void stop() {
            assert(_block_init);
            std::lock_guard<std::recursive_mutex> lck(ctrlMtx);
            if (!running) {
                return;
            }
            doStop();
            running = false;
        }

        void tempStart() {
            assert(_block_init);
            if (!tempStopDepth || --tempStopDepth) { return; }
            if (tempStopped) {
                doStart();
                tempStopped = false;
            }
        }

        void tempStop() {
            assert(_block_init);
            if (tempStopDepth++) { return; }
            if (running && !tempStopped) {
                doStop();
                tempStopped = true;
            }
        }

        virtual int run() = 0;

    protected:
        void workerLoop() {
            while (run() >= 0) {}
        }

        virtual void doStart() {
            workerThread = std::thread(&block::workerLoop, this);
        }

        virtual void doStop() {
            for (auto& in : inputs) {
                in->stopReader();
            }
            for (auto& out : outputs) {
                out->stopWriter();
            }

            // TODO: Make sure this isn't needed, I don't know why it stops
            if (workerThread.joinable()) {
                workerThread.join();
            }

            for (auto& in : inputs) {
                in->clearReadStop();
            }
            for (auto& out : outputs) {
                out->clearWriteStop();
            }
        }
    
        void acquire() {
            ctrlMtx.lock();
        }

        void release() {
            ctrlMtx.unlock();
        }

        void registerInput(untyped_stream* inStream) {
            inputs.push_back(inStream);
        }

        void unregisterInput(untyped_stream* inStream) {
            inputs.erase(std::remove(inputs.begin(), inputs.end(), inStream), inputs.end());
        }

        void registerOutput(untyped_stream* outStream) {
            outputs.push_back(outStream);
        }

        void unregisterOutput(untyped_stream* outStream) {
            outputs.erase(std::remove(outputs.begin(), outputs.end(), outStream), outputs.end());
        }

        bool _block_init = false;

        std::recursive_mutex ctrlMtx;

        std::vector<untyped_stream*> inputs;
        std::vector<untyped_stream*> outputs;

        bool running = false;
        bool tempStopped = false;
        int tempStopDepth = 0;
        std::thread workerThread;
    };
}