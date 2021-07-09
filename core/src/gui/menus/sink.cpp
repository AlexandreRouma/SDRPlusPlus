#include <gui/menus/sink.h>
#include <signal_path/signal_path.h>
#include <core.h>

namespace sinkmenu {
    void init() {
        core::configManager.acquire();
        sigpath::sinkManager.loadSinksFromConfig();
        core::configManager.release();
    }

    void draw(void* ctx) {
        sigpath::sinkManager.showMenu();
    }
};