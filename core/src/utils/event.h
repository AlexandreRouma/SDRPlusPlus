#pragma once
#include <vector>
#include <spdlog/spdlog.h>

template <class T>
struct EventHandler {
    EventHandler() {}
    EventHandler(void (*handler)(T, void*), void* ctx) {
        this->handler = handler;
        this->ctx = ctx;
    }

    void (*handler)(T, void*);
    void* ctx;
};

template <class T>
class Event {
public:
    Event() {}
    ~Event() {}

    void emit(T value) {
        for (auto const& handler : handlers) {
            handler->handler(value, handler->ctx);
        }
    }

    void bindHandler(EventHandler<T>* handler) {
        handlers.push_back(handler);
    }

    void unbindHandler(EventHandler<T>* handler) {
        if (std::find(handlers.begin(), handlers.end(), handler) == handlers.end()) {
            spdlog::error("Tried to remove a non-existent event handler");
            return;
        }
        handlers.erase(std::remove(handlers.begin(), handlers.end(), handler), handlers.end());
    }

private:
    std::vector<EventHandler<T>*> handlers;

};