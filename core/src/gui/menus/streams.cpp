#include "streams.h"
#include <signal_path/signal_path.h>
#include <imgui.h>
#include <utils/flog.h>
#include <gui/style.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

namespace streamsmenu {
    std::vector<SinkID> sinksToBeRemoved;

    void init() {
        
    }

    void draw(void* ctx) {
        float menuWidth = ImGui::GetContentRegionAvail().x;
        auto lck = sigpath::streamManager.getStreamsLock();
        auto streams = sigpath::streamManager.getStreams();

        int count = 0;
        int maxCount = streams.size();
        for (auto& [name, stream] : streams) {
            // Stream name 
            ImGui::SetCursorPosX((menuWidth / 2.0f) - (ImGui::CalcTextSize(name.c_str()).x / 2.0f));
            ImGui::Text("%s", name.c_str());

            // Display ever sink
            if (ImGui::BeginTable(CONCAT("sdrpp_streams_tbl_", name), 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                auto lck2 = stream->getSinksLock();
                auto sinks = stream->getSinks();
                for (auto& [id, sink] : sinks) {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    
                    sink->showMenu();

                    // Volume slider + mute button


                    ImGui::FillWidth();
                    if (ImGui::Button(CONCAT("Remove##sdrpp_streams_remove_type_", name))) {
                        sinksToBeRemoved.push_back(id);
                    }
                }
                lck2.unlock();

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);

                int ssds = 0;
                ImGui::Combo(CONCAT("##sdrpp_streams_add_type_", name), &ssds, "Test 1\0Test 2\0");
                ImGui::SameLine();
                ImGui::FillWidth();
                if (ImGui::Button(CONCAT("Add##sdrpp_streams_add_btn_", name))) {
                    stream->addSink("Audio");
                }

                ImGui::EndTable();

                // Remove sinks that need to be removed
                if (!sinksToBeRemoved.empty()) {
                    for (auto& id : sinksToBeRemoved) {
                        stream->removeSink(id);
                    }
                    sinksToBeRemoved.clear();
                }
            }

            count++;
            if (count < maxCount) {
                ImGui::Spacing();
            }
            ImGui::Spacing();
        }
    }
};