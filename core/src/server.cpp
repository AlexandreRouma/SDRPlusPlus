#include "server.h"
#include "core.h"
#include <spdlog/spdlog.h>
#include <version.h>
#include <config.h>
#include <options.h>
#include <filesystem>
#include <dsp/types.h>
#include <signal_path/signal_path.h>
#include <gui/smgui.h>
#include <utils/optionlist.h>
#include <dsp/compression.h>

namespace server {
    dsp::stream<dsp::complex_t> dummyInput;
    dsp::DynamicRangeCompressor comp;
    dsp::HandlerSink<uint8_t> hnd;
    net::Conn client;
    uint8_t* rbuf = NULL;
    uint8_t* sbuf = NULL;
    uint8_t* bbuf = NULL;

    PacketHeader* r_pkt_hdr = NULL;
    uint8_t* r_pkt_data = NULL;
    CommandHeader* r_cmd_hdr = NULL;
    uint8_t* r_cmd_data = NULL;

    PacketHeader* s_pkt_hdr = NULL;
    uint8_t* s_pkt_data = NULL;
    CommandHeader* s_cmd_hdr = NULL;
    uint8_t* s_cmd_data = NULL;

    PacketHeader* bb_pkt_hdr = NULL;
    uint8_t* bb_pkt_data = NULL;

    SmGui::DrawListElem dummyElem;

    net::Listener listener;

    OptionList<std::string, std::string> sourceList;
    int sourceId = 0;
    bool running = false;
    double sampleRate = 1000000.0;

    int main() {
        spdlog::info("=====| SERVER MODE |=====");

        // Init DSP
        comp.init(&dummyInput, dsp::PCM_TYPE_I8);
        hnd.init(&comp.out, _testServerHandler, NULL);
        rbuf = new uint8_t[SERVER_MAX_PACKET_SIZE];
        sbuf = new uint8_t[SERVER_MAX_PACKET_SIZE];
        bbuf = new uint8_t[SERVER_MAX_PACKET_SIZE];
        comp.start();
        hnd.start();

        // Initialize headers
        r_pkt_hdr = (PacketHeader*)rbuf;
        r_pkt_data = &rbuf[sizeof(PacketHeader)];
        r_cmd_hdr = (CommandHeader*)r_pkt_data;
        r_cmd_data = &rbuf[sizeof(PacketHeader) + sizeof(CommandHeader)];

        s_pkt_hdr = (PacketHeader*)sbuf;
        s_pkt_data = &sbuf[sizeof(PacketHeader)];
        s_cmd_hdr = (CommandHeader*)s_pkt_data;
        s_cmd_data = &sbuf[sizeof(PacketHeader) + sizeof(CommandHeader)];

        bb_pkt_hdr = (PacketHeader*)bbuf;
        bb_pkt_data = &bbuf[sizeof(PacketHeader)];

        // Terminate config manager
        core::configManager.disableAutoSave();
        core::configManager.save();

        core::configManager.acquire();
        std::string modulesDir = core::configManager.conf["modulesDirectory"];
        std::vector<std::string> modules = core::configManager.conf["modules"];
        auto modList = core::configManager.conf["moduleInstances"].items();
        core::configManager.release();
        modulesDir = std::filesystem::absolute(modulesDir).string();

        spdlog::info("Loading modules");

        // Load modules and check type to only load sources ( TODO: Have a proper type parameter int the info )
        // TODO LATER: Add whitelist/blacklist stuff
        if (std::filesystem::is_directory(modulesDir)) {
            for (const auto& file : std::filesystem::directory_iterator(modulesDir)) {
                std::string path = file.path().generic_string();
                std::string fn = file.path().filename().string();
                if (file.path().extension().generic_string() != SDRPP_MOD_EXTENTSION) {
                    continue;
                }
                if (!file.is_regular_file()) { continue; }
                if (fn.find("source") == std::string::npos) { continue; }

                spdlog::info("Loading {0}", path);
                core::moduleManager.loadModule(path);
            }
        }
        else {
            spdlog::warn("Module directory {0} does not exist, not loading modules from directory", modulesDir);
        }

        // Load additional modules through the config ( TODO: Have a proper type parameter int the info )
        // TODO LATER: Add whitelist/blacklist stuff
        for (auto const& apath : modules) {
            std::filesystem::path file = std::filesystem::absolute(apath);
            std::string path = file.generic_string();
            std::string fn = file.filename().string();
            if (file.extension().generic_string() != SDRPP_MOD_EXTENTSION) {
                continue;
            }
            if (!std::filesystem::is_regular_file(file)) { continue; }
            if (fn.find("source") == std::string::npos) { continue; }

            spdlog::info("Loading {0}", path);
            core::moduleManager.loadModule(path);
        }

        // Create module instances
        for (auto const& [name, _module] : modList) {
            std::string mod = _module["module"];
            bool enabled = _module["enabled"];
            if (core::moduleManager.modules.find(mod) == core::moduleManager.modules.end()) { continue; }
            spdlog::info("Initializing {0} ({1})", name, mod);
            core::moduleManager.createInstance(name, mod);
            if (!enabled) { core::moduleManager.disableInstance(name); }
        }

        // Do post-init
        core::moduleManager.doPostInitAll();

        // Generate source list
        auto list = sigpath::sourceManager.getSourceNames();
        for (auto& name : list) {
            sourceList.define(name, name);
        }

        // TODO: Load sourceId from config

        sigpath::sourceManager.selectSource(sourceList[sourceId]);

        // TODO: Use command line option
        listener = net::listen(options::opts.serverHost, options::opts.serverPort);
        listener->acceptAsync(_clientHandler, NULL);

        spdlog::info("Ready, listening on {0}:{1}", options::opts.serverHost, options::opts.serverPort);
        while(1) { std::this_thread::sleep_for(std::chrono::milliseconds(100)); }

        return 0;
    }

    void _clientHandler(net::Conn conn, void* ctx) {
        spdlog::info("Connection from {0}:{1}", "TODO", "TODO");
        client = std::move(conn);
        client->readAsync(sizeof(PacketHeader), rbuf, _packetHandler, NULL);

        // Perform settings reset
        sigpath::sourceManager.stop();
        comp.setPCMType(dsp::PCM_TYPE_I16);

        sendSampleRate(sampleRate);

        // TODO: Wait otherwise someone else could connect

        listener->acceptAsync(_clientHandler, NULL);
    }

    void _packetHandler(int count, uint8_t* buf, void* ctx) {
        PacketHeader* hdr = (PacketHeader*)buf;

        // Read the rest of the data (TODO: CHECK SIZE OR SHIT WILL BE FUCKED + ADD TIMEOUT)
        int len = 0;
        int read = 0;
        int goal = hdr->size - sizeof(PacketHeader);
        while (len < goal) {
            read = client->read(goal - len, &buf[sizeof(PacketHeader) + len]);
            if (read < 0) { return; };
            len += read;
        }

        // Parse and process
        if (hdr->type == PACKET_TYPE_COMMAND && hdr->size >= sizeof(PacketHeader) + sizeof(CommandHeader)) {
            CommandHeader* chdr = (CommandHeader*)&buf[sizeof(PacketHeader)];
            commandHandler((Command)chdr->cmd, &buf[sizeof(PacketHeader) + sizeof(CommandHeader)], hdr->size - sizeof(PacketHeader) - sizeof(CommandHeader));
        }
        else {
            sendError(ERROR_INVALID_PACKET);
        }

        // Start another async read
        client->readAsync(sizeof(PacketHeader), rbuf, _packetHandler, NULL);
    }

    // void _testServerHandler(dsp::complex_t* data, int count, void* ctx) {
    //     // Build data packet
    //     PacketHeader* hdr = (PacketHeader*)bbuf;
    //     hdr->type = PACKET_TYPE_BASEBAND;
    //     hdr->size = sizeof(PacketHeader) + (count * sizeof(dsp::complex_t));
    //     memcpy(&bbuf[sizeof(PacketHeader)], data, count * sizeof(dsp::complex_t));

    //     // Write to network
    //     if (client && client->isOpen()) { client->write(hdr->size, bbuf); }
    // }

    void _testServerHandler(uint8_t* data, int count, void* ctx) {
        // Build data packet
        PacketHeader* hdr = (PacketHeader*)bbuf;
        hdr->type = PACKET_TYPE_BASEBAND;
        hdr->size = sizeof(PacketHeader) + count;
        memcpy(&bbuf[sizeof(PacketHeader)], data, count);

        // Write to network
        if (client && client->isOpen()) { client->write(hdr->size, bbuf); }
    }

    void setInput(dsp::stream<dsp::complex_t>* stream) {
        comp.setInput(stream);
    }

    void commandHandler(Command cmd, uint8_t* data, int len) {
        if (cmd == COMMAND_GET_UI) {
            sendUI(COMMAND_GET_UI, "", dummyElem);
        }
        else if (cmd == COMMAND_UI_ACTION && len >= 3) {
            // Check if sending back data is needed
            int i = 0;
            bool sendback = data[i++];
            len--;
            
            // Load id
            SmGui::DrawListElem diffId;
            int count = SmGui::DrawList::loadItem(diffId, &data[i], len);
            if (count < 0) { sendError(ERROR_INVALID_ARGUMENT); return; }
            if (diffId.type != SmGui::DRAW_LIST_ELEM_TYPE_STRING) { sendError(ERROR_INVALID_ARGUMENT); return; } 
            i += count;
            len -= count;

            // Load value
            SmGui::DrawListElem diffValue;
            count = SmGui::DrawList::loadItem(diffValue, &data[i], len);
            if (count < 0) { sendError(ERROR_INVALID_ARGUMENT); return; }
            i += count;
            len -= count;

            // Render and send back
            if (sendback) {
                sendUI(COMMAND_UI_ACTION, diffId.str, diffValue);
            }
            else {
                renderUI(NULL, diffId.str, diffValue);
            }
        }
        else if (cmd == COMMAND_START) {
            sigpath::sourceManager.start();
            running = true;
        }
        else if (cmd == COMMAND_STOP) {
            sigpath::sourceManager.stop();
            running = false;
        }
        else if (cmd == COMMAND_SET_FREQUENCY && len == 8) {
            sigpath::sourceManager.tune(*(double*)data);
            sendCommandAck(COMMAND_SET_FREQUENCY, 0);
        }
        else if (cmd == COMMAND_SET_SAMPLE_TYPE && len == 1) {
            dsp::PCMType type = (dsp::PCMType)*(uint8_t*)data;
            comp.setPCMType(type);
        }
        else {
            spdlog::error("Invalid Command: {0} (len = {1})", cmd, len);
            sendError(ERROR_INVALID_COMMAND);
        }
    }

    void drawMenu() {
        if (running) { SmGui::BeginDisabled(); }
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo("##sdrpp_server_src_sel", &sourceId, sourceList.txt)) {
            sigpath::sourceManager.selectSource(sourceList[sourceId]);
            // TODO: Save config
        }
        if (running) { SmGui::EndDisabled(); }

        sigpath::sourceManager.showSelectedMenu();
    }

    void renderUI(SmGui::DrawList* dl, std::string diffId, SmGui::DrawListElem diffValue) {
        // If we're recording and there's an action, render once with the action and record without

        if (dl && !diffId.empty()) {
            SmGui::setDiff(diffId, diffValue);
            drawMenu();

            SmGui::setDiff("", dummyElem);
            SmGui::startRecord(dl);
            drawMenu();
            SmGui::stopRecord();
        }
        else {
            SmGui::setDiff(diffId, diffValue);
            SmGui::startRecord(dl);
            drawMenu();
            SmGui::stopRecord();
        }
    }

    void sendUI(Command originCmd, std::string diffId, SmGui::DrawListElem diffValue) {
        // Render UI
        SmGui::DrawList dl;
        renderUI(&dl, diffId, diffValue);

        // Create response
        int size = dl.getSize();
        dl.store(s_cmd_data, size);

        // Send to network
        sendCommandAck(originCmd, size);
    }

    void sendError(Error err) {
        PacketHeader* hdr = (PacketHeader*)sbuf;
        s_pkt_data[0] = err;
        sendPacket(PACKET_TYPE_ERROR, 1);
    }

    void sendSampleRate(double sampleRate) {
        *(double*)s_cmd_data = sampleRate;
        sendCommand(COMMAND_SET_SAMPLERATE, sizeof(double));
    }

    void setInputSampleRate(double samplerate) {
        sampleRate = samplerate;
        if (!client || !client->isOpen()) { return; }
        sendSampleRate(sampleRate);
    }

    void sendPacket(PacketType type, int len) {
        s_pkt_hdr->type = type;
        s_pkt_hdr->size = sizeof(PacketHeader) + len;
        client->write(s_pkt_hdr->size, sbuf);
    }

    void sendCommand(Command cmd, int len) {
        s_cmd_hdr->cmd = cmd;
        sendPacket(PACKET_TYPE_COMMAND, sizeof(CommandHeader) + len);
    }

    void sendCommandAck(Command cmd, int len) {
        s_cmd_hdr->cmd = cmd;
        sendPacket(PACKET_TYPE_COMMAND_ACK, sizeof(CommandHeader) + len);
    }
}
