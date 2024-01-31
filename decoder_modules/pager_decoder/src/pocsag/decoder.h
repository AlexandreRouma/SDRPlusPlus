#pragma once
#include "../decoder.h"
#include <utils/optionlist.h>
#include <gui/widgets/symbol_diagram.h>
#include <gui/style.h>

class POCSAGDecoder : public Decoder {
public:
    POCSAGDecoder() : diag(0.6, 2400) {
        // Define baudrate options
        baudrates.define(512, "512 Baud", 512);
        baudrates.define(1200, "1200 Baud", 1200);
        baudrates.define(2400, "2400 Baud", 2400);
    }

    void showMenu() {
        ImGui::LeftLabel("Baudrate");
        ImGui::FillWidth();
        if (ImGui::Combo(("##pager_decoder_proto_" + name).c_str(), &brId, baudrates.txt)) {
            // TODO
        }
    }

private:
    std::string name;

    ImGui::SymbolDiagram diag;

    int brId = 2;

    OptionList<int, int> baudrates;
};