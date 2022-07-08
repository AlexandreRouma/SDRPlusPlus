#pragma once
#include "block.h"

namespace dsp {
    class hier_block : public generic_block {
    public:
        virtual void init() {}

        virtual ~hier_block() {
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

    private:
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

        std::vector<generic_block*> blocks;
        bool tempStopped = false;
        bool running = false;
        int tempStopDepth = 0;

    protected:
        void registerBlock(generic_block* block) {
            blocks.push_back(block);
        }

        void unregisterBlock(generic_block* block) {
            blocks.erase(std::remove(blocks.begin(), blocks.end(), block), blocks.end());
        }

        bool _block_init = false;
        std::recursive_mutex ctrlMtx;
    };
}