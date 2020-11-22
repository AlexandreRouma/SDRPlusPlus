#include <gui/menus/sink.h>
#include <signal_path/signal_path.h>

namespace sinkmenu {
    void init() {

    }

    void draw(void* ctx) {
        sigpath::sinkManager.showMenu();
    }
};