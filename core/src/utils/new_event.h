#pragma once
#include <functional>
#include <stdexcept>
#include <mutex>
#include <map>

typedef int HandlerID;

template <typename... Args>
class NewEvent {
public:
    using Handler = std::function<void(Args...)>;

    HandlerID bind(const Handler& handler) {
        std::lock_guard<std::mutex> lck(mtx);
        HandlerID id = genID();
        handlers[id] = handler;
        return id;
    }

    template<typename MHandler, class T>
    HandlerID bind(MHandler handler, T* ctx) {
        return bind([=](Args... args){
            (ctx->*handler)(args...);
        });
    }

    void unbind(HandlerID id) {
        std::lock_guard<std::mutex> lck(mtx);
        if (handlers.find(id) == handlers.end()) {
            throw std::runtime_error("Could not unbind handler, unknown ID");
        }
        handlers.erase(id);
    }

    void operator()(Args... args) {
        std::lock_guard<std::mutex> lck(mtx);
        for (const auto& [desc, handler] : handlers) {
            handler(args...);
        }
    }

private:
    HandlerID genID() {
        int id;
        for (id = 1; handlers.find(id) != handlers.end(); id++);
        return id;
    }

    std::map<HandlerID, Handler> handlers;
    std::mutex mtx;
};