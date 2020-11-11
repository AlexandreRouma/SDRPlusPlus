#pragma once
#include <vector>
#include <spdlog/spdlog.h>

template <class T>
class Event {
public:
    Event() {}
    ~Event() {}

    struct EventHandler {
        EventHandler(void (*handler)(T, void*), void* ctx) {
            this->handler = handler;
            this->ctx = ctx;
        }

        void (*handler)(T, void*);
        void* ctx;
    };

    void emit(T value) {
        for (auto const& handler : handlers) {
            handler.handler(value, handler.ctx);
        }
    }

    void bindHandler(const EventHandler& handler) {
        handlers.push_back(handler);
    }

    void unbindHandler(const EventHandler& handler) {
        if (handlers.find(handler) == handlers.end()) {
            spdlog::error("Tried to remove a non-existant event handler");
            return;
        }
        handlers.erase(std::remove(handlers.begin(), handlers.end(), handler), handlers.end());
    }

private:
    std::vector<EventHandler> handlers;

};