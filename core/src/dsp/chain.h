#pragma once
#include <dsp/stream.h>
#include <type_traits>
#include <vector>
#include <utils/event.h>

namespace dsp {
    template <class T>
    class ChainLinkAny {
    public:
        virtual ~ChainLinkAny() {}
        virtual void setInput(stream<T>* stream) = 0;
        virtual stream<T>* getOutput() = 0;
        virtual void start() = 0;
        virtual void stop() = 0;
        bool enabled = false;
    };

    template <class BLOCK, class T>
    class ChainLink : public ChainLinkAny<T> {
    public:
        ~ChainLink() {}
        
        void setInput(stream<T>* stream) {
            block.setInput(stream);
        }
        
        stream<T>* getOutput() {
            return &block.out;
        }

        void start() {
            block.start();
        }

        void stop() {
            block.stop();
        }

        BLOCK block;
    };

    template <class T>
    class Chain {
    public:
        Chain() {}

        Chain(stream<T>* input, EventHandler<stream<T>*>* outputChangedHandler) {
            init(input, outputChangedHandler);
        }

        void init(stream<T>* input, EventHandler<stream<T>*>* outputChangedHandler) {
            _input = input;
            onOutputChanged.bindHandler(outputChangedHandler);
        }

        void add(ChainLinkAny<T>* link) {
            // Check that link exists
            if (std::find(links.begin(), links.end(), link) != links.end()) {
                spdlog::error("Could not add new link to the chain, link already in the chain");
                return;
            }
            
            // Assert that the link is stopped and disabled
            link->stop();
            link->enabled = false;

            // Add new link to the list
            links.push_back(link);
        }

        void enable(ChainLinkAny<T>* link) {
            // Check that link exists and locate it
            auto lnit = std::find(links.begin(), links.end(), link);
            if (lnit == links.end()) {
                spdlog::error("Could not enable link");
                return;
            }
            // Enable the link
            link->enabled = true;

            // Find input
            stream<T>* input = _input;
            for (auto i = links.begin(); i < lnit; i++) {
                if (!(*i)->enabled) { continue; }
                input = (*i)->getOutput();
            }
            
            // Find next block
            ChainLinkAny<T>* nextLink = NULL;
            for (auto i = ++lnit; i < links.end(); i++) {
                if (!(*i)->enabled) { continue; }
                nextLink = *i;
            }

            if (nextLink) {
                // If a next block exists, change its input
                nextLink->setInput(link->getOutput());
            }
            else {
                // If there are no next blocks, change output of outside reader
                onOutputChanged.emit(link->getOutput());
            }

            // Set input of newly enabled link
            link->setInput(input);

            // If running, start everything
            if (running) { start(); }
        }

        void disable(ChainLinkAny<T>* link) {
            // Check that link exists and locate it
            auto lnit = std::find(links.begin(), links.end(), link);
            if (lnit == links.end()) {
                spdlog::error("Could not disable link");
                return;
            }

            // Stop and disable link
            link->stop();
            link->enabled = false;

            // Find its input
            stream<T>* input = _input;
            for (auto i = links.begin(); i < lnit; i++) {
                if (!(*i)->enabled) { continue; }
                input = (*i)->getOutput();
            }
            
            // Find next block
            ChainLinkAny<T>* nextLink = NULL;
            for (auto i = ++lnit; i < links.end(); i++) {
                if (!(*i)->enabled) { continue; }
                nextLink = *i;
            }

            if (nextLink) {
                // If a next block exists, change its input
                nextLink->setInput(input);
            }
            else {
                // If there are no next blocks, change output of outside reader
                onOutputChanged.emit(input);
            }
        }

        void disableAll() {
            for (auto& ln : links) {
                disable(ln);
            }
        }

        void setState(ChainLinkAny<T>* link, bool enabled) {
            enabled ? enable(link) : disable(link);
        }

        void setInput(stream<T>* input) {
            _input = input;
            
            // Set input of first enabled link
            for (auto& ln : links) {
                if (!ln->enabled) { continue; }
                ln->setInput(_input);
                return;
            }

            // No block found, this means nothing is enabled
            onOutputChanged.emit(_input);
        }

        stream<T>* getOutput() {
            stream<T>* lastOutput = _input;
            for (auto& ln : links) {
                if (!ln->enabled) { continue; }
                lastOutput = ln->getOutput();
            }
            return lastOutput;
        }

        void start() {
            running = true;
            for (auto& ln : links) {
                if (ln->enabled) {
                    ln->start();
                }
                else {
                    ln->stop();
                }
            }
        }

        void stop() {
            running = false;
            for (auto& ln : links) {
                ln->stop();
            }
        }

        Event<stream<T>*> onOutputChanged;

    private:
        stream<T>* _input;
        std::vector<ChainLinkAny<T>*> links;
        bool running = false;

    };
}