#include "streams.h"
#include <signal_path/signal_path.h>
#include <imgui.h>
#include <utils/flog.h>
#include <gui/style.h>
#include <gui/icons.h>
#include <utils/optionlist.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

namespace streamsmenu {
    std::vector<SinkID> sinksToBeRemoved;

    std::recursive_mutex sinkTypesMtx;
    OptionList<std::string, std::string> sinkTypes;

    std::map<std::string, int> selectedSinkTypeId;
    std::map<std::string, int> addSinkTypeId;

    int addType = 0;

    void updateSinkTypeList(const std::string& removed = "") {
        std::lock_guard<std::recursive_mutex> lck1(sinkTypesMtx);
        auto lck2 = sigpath::streamManager.getSinkTypesLock();
        const auto& types = sigpath::streamManager.getSinkTypes();
        sinkTypes.clear();
        for (const auto& type : types) {
            if (type == removed) { continue; }
            sinkTypes.define(type, type, type);
        }
    }

    void onSinkProviderRegistered(const std::string& type) {
        // Update the list
        updateSinkTypeList();

        // Update the ID of the Add dropdown
        // TODO

        // Update the selected ID of each drop down
        // TODO
    }

    void onSinkProviderUnregister(const std::string& type) {
        // Update the list
        updateSinkTypeList(type);

        // Update the ID of the Add dropdown
        // TODO

        // Update the selected ID of each drop down
        // TODO
    }

    void init() {
        sigpath::streamManager.onSinkProviderRegistered.bind(onSinkProviderRegistered);
        sigpath::streamManager.onSinkProviderUnregister.bind(onSinkProviderUnregister);
        updateSinkTypeList();
    }

    void draw(void* ctx) {
        float menuWidth = ImGui::GetContentRegionAvail().x;
        auto lck = sigpath::streamManager.getStreamsLock();
        const auto& streams = sigpath::streamManager.getStreams();

        int count = 0;
        int maxCount = streams.size();
        for (auto& [name, stream] : streams) {
            // Stream name 
            ImGui::SetCursorPosX((menuWidth / 2.0f) - (ImGui::CalcTextSize(name.c_str()).x / 2.0f));
            ImGui::Text("%s", name.c_str());

            // Display ever sink
            if (ImGui::BeginTable(CONCAT("sdrpp_streams_tbl_", name), 1, ImGuiTableFlags_Borders)) {
                auto lck2 = stream->getSinksLock();
                auto sinks = stream->getSinks();
                for (auto& [id, sink] : sinks) {
                    std::string sid = sink->getStringID();
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    float tableWidth = ImGui::GetContentRegionAvail().x;
                    
                    ImGui::Spacing();

                    // Sink type
                    int ttttt = 0;
                    ImGui::FillWidth();
                    if (ImGui::Combo(CONCAT("##sdrpp_streams_type_", sid), &ttttt, sinkTypes.txt)) {
                        
                    }

                    sink->showMenu();
                    float vol = sink->getVolume();
                    bool muted = sink->getMuted();
                    float pan = sink->getPanning();
                    bool linked = true;

                    float height = ImGui::GetTextLineHeightWithSpacing() + 2;
                    ImGui::PushID(ImGui::GetID(("sdrpp_streams_center_btn_" + sid).c_str()));
                    if (ImGui::ImageButton(icons::ALIGN_CENTER, ImVec2(height, height), ImVec2(0, 0), ImVec2(1, 1), 0, ImVec4(0, 0, 0, 0), ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                        sink->setPanning(0.0f);
                    }
                    ImGui::PopID();
                    ImGui::SameLine();
                    ImGui::FillWidth();
                    if (ImGui::SliderFloat(CONCAT("##sdrpp_streams_pan_", sid), &pan, -1, 1, "")) {
                        sink->setPanning(pan);
                    }
                    
                    if (muted) {
                        ImGui::PushID(ImGui::GetID(("sdrpp_unmute_btn_" + sid).c_str()));
                        if (ImGui::ImageButton(icons::MUTED, ImVec2(height, height), ImVec2(0, 0), ImVec2(1, 1), 0, ImVec4(0, 0, 0, 0), ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                            sink->setMuted(false);
                        }
                        ImGui::PopID();
                    }
                    else {
                        ImGui::PushID(ImGui::GetID(("sdrpp_mute_btn_" + sid).c_str()));
                        if (ImGui::ImageButton(icons::UNMUTED, ImVec2(height, height), ImVec2(0, 0), ImVec2(1, 1), 0, ImVec4(0, 0, 0, 0), ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
                            sink->setMuted(true);
                        }
                        ImGui::PopID();
                    }
                    ImGui::SameLine();
                    ImGui::FillWidth();
                    if (ImGui::SliderFloat(CONCAT("##sdrpp_streams_vol_", sid), &vol, 0, 1, "")) {
                        sink->setVolume(vol);
                    }


                    int startCur = ImGui::GetCursorPosX();
                    if (ImGui::Checkbox(CONCAT("Link volume##sdrpp_streams_vol_", sid), &linked)) {
                        // TODO
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(CONCAT("Remove##sdrpp_streams_remove_type_", sid), ImVec2(tableWidth - (ImGui::GetCursorPosX() - startCur), 0))) {
                        sinksToBeRemoved.push_back(id);
                    }
                    ImGui::Spacing();
                }
                lck2.unlock();

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                float tableWidth = ImGui::GetContentRegionAvail().x;

                ImGui::Spacing();
                int startCur = ImGui::GetCursorPosX();
                {
                    std::lock_guard<std::recursive_mutex> lck(sinkTypesMtx);
                    ImGui::Combo(CONCAT("##sdrpp_streams_add_type_", name), &addType, sinkTypes.txt);
                    ImGui::SameLine();
                    if (ImGui::Button(CONCAT("Add##sdrpp_streams_add_btn_", name), ImVec2(tableWidth - (ImGui::GetCursorPosX() - startCur), 0))) {
                        stream->addSink(sinkTypes.value(addType));
                    }
                    ImGui::Spacing();
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