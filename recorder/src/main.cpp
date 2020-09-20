#include <imgui.h>
#include <module.h>
#include <watcher.h>
#include <wav.h>
#include <dsp/types.h>
#include <dsp/stream.h>
#include <thread>
#include <ctime>
#include <signal_path/audio.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

struct RecorderContext_t {
    std::string name;
    dsp::stream<dsp::StereoFloat_t>* stream;
    WavWriter* writer;
    std::thread workerThread;
    bool recording;
    time_t startTime;
    std::string lastNameList;
    std::string selectedStreamName;
    int selectedStreamId;
    uint64_t samplesWritten;
    float sampleRate;
};

void _writeWorker(RecorderContext_t* ctx) {
    dsp::StereoFloat_t* floatBuf = new dsp::StereoFloat_t[1024];
    int16_t* sampleBuf = new int16_t[2048];
    while (true) {
        if (ctx->stream->read(floatBuf, 1024) < 0) {
            break;
        }
        for (int i = 0; i < 1024; i++) {
            sampleBuf[(i * 2) + 0] = floatBuf[i].l * 0x7FFF;
            sampleBuf[(i * 2) + 1] = floatBuf[i].r * 0x7FFF;
        }
        ctx->samplesWritten += 1024;
        ctx->writer->writeSamples(sampleBuf, 2048 * sizeof(int16_t));
    }
    delete[] floatBuf;
    delete[] sampleBuf;
}

std::string genFileName(std::string prefix) {
    time_t now = time(0);
    tm *ltm = localtime(&now);
    char buf[1024];
    sprintf(buf, "%02d-%02d-%02d_%02d-%02d-%02d.wav", ltm->tm_hour, ltm->tm_min, ltm->tm_sec, ltm->tm_mday, ltm->tm_mon + 1, ltm->tm_year + 1900);
    return prefix + buf;
}

void streamRemovedHandler(void* ctx) {

}

void sampleRateChanged(void* ctx, float sampleRate, int blockSize) {

}

MOD_EXPORT void* _INIT_(mod::API_t* _API, ImGuiContext* imctx, std::string _name) {
    RecorderContext_t* ctx = new RecorderContext_t;
    ctx->recording = false;
    ctx->selectedStreamName = "";
    ctx->selectedStreamId = 0;
    ctx->lastNameList = "";
    ImGui::SetCurrentContext(imctx);
    return ctx;
}

MOD_EXPORT void _NEW_FRAME_(RecorderContext_t* ctx) {
    
}

MOD_EXPORT void _DRAW_MENU_(RecorderContext_t* ctx) {
    float menuColumnWidth = ImGui::GetContentRegionAvailWidth();

    std::vector<std::string> streamNames = audio::getStreamNameList();
    std::string nameList;
    for (std::string name : streamNames) {
        nameList += name;
        nameList += '\0';
    }

    if (nameList == "") {
        ImGui::Text("No audio stream available");
        return;
    }

    if (ctx->lastNameList != nameList) {
        ctx->lastNameList = nameList;
        auto _nameIt = std::find(streamNames.begin(), streamNames.end(), ctx->selectedStreamName);
        if (_nameIt == streamNames.end()) {
            // TODO: verify if there even is a stream
            ctx->selectedStreamId = 0;
            ctx->selectedStreamName = streamNames[ctx->selectedStreamId];
        }
        else {
            ctx->selectedStreamId = std::distance(streamNames.begin(), _nameIt);
            ctx->selectedStreamName = streamNames[ctx->selectedStreamId];
        }
    }

    ImGui::PushItemWidth(menuColumnWidth);
    if (!ctx->recording) {
        if (ImGui::Combo(CONCAT("##_strea_select_", ctx->name), &ctx->selectedStreamId, nameList.c_str())) {
            ctx->selectedStreamName = streamNames[ctx->selectedStreamId];
        }
    }
    else {
        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.44f, 0.44f, 0.44f, 0.15f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.20f, 0.21f, 0.22f, 0.30f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.00f, 1.00f, 1.00f, 0.65f));
        ImGui::Combo(CONCAT("##_strea_select_", ctx->name), &ctx->selectedStreamId, nameList.c_str());
        ImGui::PopItemFlag();
        ImGui::PopStyleColor(3);
    }
    
    if (!ctx->recording) {
        if (ImGui::Button("Record", ImVec2(menuColumnWidth, 0))) {
            ctx->samplesWritten = 0;
            ctx->sampleRate = 48000;
            ctx->writer = new WavWriter("recordings/" + genFileName("audio_"), 16, 2, 48000);
            ctx->stream = audio::bindToStreamStereo(ctx->selectedStreamName, streamRemovedHandler, sampleRateChanged, ctx);
            ctx->workerThread = std::thread(_writeWorker, ctx);
            ctx->recording = true;
            ctx->startTime = time(0);
        }
        ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_Text), "Idle --:--:--");
    }
    else {
        if (ImGui::Button("Stop", ImVec2(menuColumnWidth, 0))) {
            ctx->stream->stopReader();
            ctx->workerThread.join();
            ctx->stream->clearReadStop();
            audio::unbindFromStreamStereo(ctx->selectedStreamName, ctx->stream);
            ctx->writer->close();
            delete ctx->writer;
            ctx->recording = false;
        }
        uint64_t seconds = ctx->samplesWritten / (uint64_t)ctx->sampleRate;
        time_t diff = seconds;
        tm *dtm = gmtime(&diff);
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Recording %02d:%02d:%02d", dtm->tm_hour, dtm->tm_min, dtm->tm_sec);
    }
}

MOD_EXPORT void _HANDLE_EVENT_(RecorderContext_t* ctx, int eventId) {
    // INSTEAD OF EVENTS, REGISTER HANDLER WHEN CREATING VFO
    if (eventId == mod::EVENT_STREAM_PARAM_CHANGED) {
        
    }
    else if (eventId == mod::EVENT_SELECTED_VFO_CHANGED) {
        
    }
}

MOD_EXPORT void _STOP_(RecorderContext_t* ctx) {
    
}