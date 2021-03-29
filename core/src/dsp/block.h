#pragma once
#include <stdio.h>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <thread>
#include <vector>
#include <algorithm>

#include <spdlog/spdlog.h>

#define FL_M_PI                3.1415926535f

namespace dsp {

    class generic_unnamed_block {
    public:
        virtual void start() {}
        virtual void stop() {}
        virtual int calcOutSize(int inSize) { return inSize; }
        virtual int run() { return -1; }
    };
    
    template <class BLOCK>
    class generic_block : public generic_unnamed_block {
    public:
        virtual void init() {}

        virtual ~generic_block() {
            stop();
        }

        virtual void start() {
            std::lock_guard<std::mutex> lck(ctrlMtx);
            if (running) {
                return;
            }
            running = true;
            doStart();
        }

        virtual void stop() {
            std::lock_guard<std::mutex> lck(ctrlMtx);
            if (!running) {
                return;
            }
            doStop();
            running = false;
        }

        virtual int calcOutSize(int inSize) { return inSize; }

        virtual int run() = 0;
        
        friend BLOCK;

    private:
        void workerLoop() { 
            while (run() >= 0);
        }

        void aquire() {
            ctrlMtx.lock();
        }

        void release() {
            ctrlMtx.unlock();
        }

        void registerInput(untyped_steam* inStream) {
            inputs.push_back(inStream);
        }

        void unregisterInput(untyped_steam* inStream) {
            inputs.erase(std::remove(inputs.begin(), inputs.end(), inStream), inputs.end());
        }

        void registerOutput(untyped_steam* outStream) {
            outputs.push_back(outStream);
        }

        void unregisterOutput(untyped_steam* outStream) {
            outputs.erase(std::remove(outputs.begin(), outputs.end(), outStream), outputs.end());
        }

        virtual void doStart() {
            workerThread = std::thread(&generic_block<BLOCK>::workerLoop, this);
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

        void tempStart() {
            if (tempStopped) {
                doStart();
                tempStopped = false;
            }
        }

        void tempStop() {
            if (running && !tempStopped) {
                doStop();
                tempStopped = true;
            }
        }

        std::vector<untyped_steam*> inputs;
        std::vector<untyped_steam*> outputs;

        bool running = false;
        bool tempStopped = false;

        std::thread workerThread;

    protected:
        std::mutex ctrlMtx;

    };

    template <class BLOCK>
    class generic_hier_block {
    public:
        virtual void init() {}

        virtual ~generic_hier_block() {
            stop();
        }

        virtual void start() {
            std::lock_guard<std::mutex> lck(ctrlMtx);
            if (running) {
                return;
            }
            running = true;
            doStart();
        }

        virtual void stop() {
            std::lock_guard<std::mutex> lck(ctrlMtx);
            if (!running) {
                return;
            }
            doStop();
            running = false;
        }

        virtual int calcOutSize(int inSize) { return inSize; }

        friend BLOCK;

    private:
        void registerBlock(generic_unnamed_block* block) {
            blocks.push_back(block);
        }

        void unregisterBlock(generic_unnamed_block* block) {
            blocks.erase(std::remove(blocks.begin(), blocks.end(), block), blocks.end());
        }

        virtual void doStart() {
            for (auto& block : blocks) {
                block->start();
            }
        }

        virtual void doStop() {
            for (auto& block : blocks) {
                block->stop();
            }
        }

        void tempStart() {
            if (tempStopped) {
                doStart();
                tempStopped = false;
            }
        }

        void tempStop() {
            if (running && !tempStopped) {
                doStop();
                tempStopped = true;
            }
        }

        std::vector<generic_unnamed_block*> blocks;
        bool tempStopped = false;
        bool running = false;

    protected:
        std::mutex ctrlMtx;

    };
}