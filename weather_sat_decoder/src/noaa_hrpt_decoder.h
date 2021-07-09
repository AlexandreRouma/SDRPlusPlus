#pragma once
#include <sat_decoder.h>
#include <dsp/demodulator.h>
#include <dsp/deframing.h>
#include <dsp/noaa/hrpt.h>
#include <dsp/noaa/tip.h>
#include <dsp/sink.h>
#include <gui/widgets/symbol_diagram.h>
#include <gui/widgets/line_push_image.h>

#define NOAA_HRPT_VFO_SR    3000000.0f
#define NOAA_HRPT_VFO_BW    2000000.0f

class NOAAHRPTDecoder : public SatDecoder {
public:
    NOAAHRPTDecoder(VFOManager::VFO* vfo, std::string name) : avhrrRGBImage(2048, 256), avhrr1Image(2048, 256), avhrr2Image(2048, 256), avhrr3Image(2048, 256), avhrr4Image(2048, 256), avhrr5Image(2048, 256), symDiag(0.5f) {
        _vfo = vfo;
        _name = name;

        // Core DSP
        demod.init(vfo->output, NOAA_HRPT_VFO_SR, 665400.0f * 2.0f, 0.02e-3, (0.06f*0.06f) / 2.0f, 32, 0.6f, (0.01f*0.01f) / 4.0f, 0.01f, 0.005);
        
        split.init(demod.out);
        split.bindStream(&dataStream);
        split.bindStream(&visStream);

        reshape.init(&visStream, 1024, (NOAA_HRPT_VFO_SR/30) - 1024);
        visSink.init(&reshape.out, visHandler, this);
        
        deframe.init(&dataStream, 11090 * 10 * 2, (uint8_t*)dsp::noaa::HRPTSyncWord, 60);
        manDec.init(&deframe.out, false);
        packer.init(&manDec.out);
        demux.init(&packer.out);
        tipDemux.init(&demux.TIPOut);
        hirsDemux.init(&tipDemux.HIRSOut);

        // All Sinks
        avhrr1Sink.init(&demux.AVHRRChan1Out, avhrr1Handler, this);
        avhrr2Sink.init(&demux.AVHRRChan2Out, avhrr2Handler, this);
        avhrr3Sink.init(&demux.AVHRRChan3Out, avhrr3Handler, this);
        avhrr4Sink.init(&demux.AVHRRChan4Out, avhrr4Handler, this);
        avhrr5Sink.init(&demux.AVHRRChan5Out, avhrr5Handler, this);

        sbuvSink.init(&tipDemux.SBUVOut);
        dcsSink.init(&tipDemux.DCSOut);
        semSink.init(&tipDemux.SEMOut);

        aipSink.init(&demux.AIPOut);

        hirs1Sink.init(&hirsDemux.radChannels[0], hirs1Handler, this);
        hirs2Sink.init(&hirsDemux.radChannels[1], hirs2Handler, this);
        hirs3Sink.init(&hirsDemux.radChannels[2], hirs3Handler, this);
        hirs4Sink.init(&hirsDemux.radChannels[3], hirs4Handler, this);
        hirs5Sink.init(&hirsDemux.radChannels[4], hirs5Handler, this);
        hirs6Sink.init(&hirsDemux.radChannels[5], hirs6Handler, this);
        hirs7Sink.init(&hirsDemux.radChannels[6], hirs7Handler, this);
        hirs8Sink.init(&hirsDemux.radChannels[7], hirs8Handler, this);
        hirs9Sink.init(&hirsDemux.radChannels[8], hirs9Handler, this);
        hirs10Sink.init(&hirsDemux.radChannels[9], hirs10Handler, this);
        hirs11Sink.init(&hirsDemux.radChannels[10], hirs11Handler, this);
        hirs12Sink.init(&hirsDemux.radChannels[11], hirs12Handler, this);
        hirs13Sink.init(&hirsDemux.radChannels[12], hirs13Handler, this);
        hirs14Sink.init(&hirsDemux.radChannels[13], hirs14Handler, this);
        hirs15Sink.init(&hirsDemux.radChannels[14], hirs15Handler, this);
        hirs16Sink.init(&hirsDemux.radChannels[15], hirs16Handler, this);
        hirs17Sink.init(&hirsDemux.radChannels[16], hirs17Handler, this);
        hirs18Sink.init(&hirsDemux.radChannels[17], hirs18Handler, this);
        hirs19Sink.init(&hirsDemux.radChannels[18], hirs19Handler, this);
        hirs20Sink.init(&hirsDemux.radChannels[19], hirs20Handler, this);
    }

    void select() {
        _vfo->setSampleRate(NOAA_HRPT_VFO_SR, NOAA_HRPT_VFO_BW);
        _vfo->setReference(ImGui::WaterfallVFO::REF_CENTER);
        _vfo->setBandwidthLimits(NOAA_HRPT_VFO_BW, NOAA_HRPT_VFO_BW, true);
    };

    void start() {
        demod.start();

        split.start();
        reshape.start();
        visSink.start();

        deframe.start();
        manDec.start();
        packer.start();
        demux.start();
        tipDemux.start();
        hirsDemux.start();

        avhrr1Sink.start();
        avhrr2Sink.start();
        avhrr3Sink.start();
        avhrr4Sink.start();
        avhrr5Sink.start();

        sbuvSink.start();
        dcsSink.start();
        semSink.start();

        aipSink.start();

        hirs1Sink.start();
        hirs2Sink.start();
        hirs3Sink.start();
        hirs4Sink.start();
        hirs5Sink.start();
        hirs6Sink.start();
        hirs7Sink.start();
        hirs8Sink.start();
        hirs9Sink.start();
        hirs10Sink.start();
        hirs11Sink.start();
        hirs12Sink.start();
        hirs13Sink.start();
        hirs14Sink.start();
        hirs15Sink.start();
        hirs16Sink.start();
        hirs17Sink.start();
        hirs18Sink.start();
        hirs19Sink.start();
        hirs20Sink.start();

        compositeThread = std::thread(&NOAAHRPTDecoder::avhrrCompositeWorker, this);
    };
    
    void stop() {
        compositeIn1.stopReader();
        compositeIn1.stopWriter();
        compositeIn2.stopReader();
        compositeIn2.stopWriter();

        demod.stop();

        split.stop();
        reshape.stop();
        visSink.stop();

        deframe.stop();
        manDec.stop();
        packer.stop();
        demux.stop();
        tipDemux.stop();
        hirsDemux.stop();

        avhrr1Sink.stop();
        avhrr2Sink.stop();
        avhrr3Sink.stop();
        avhrr4Sink.stop();
        avhrr5Sink.stop();

        sbuvSink.stop();
        dcsSink.stop();
        semSink.stop();

        aipSink.stop();

        hirs1Sink.stop();
        hirs2Sink.stop();
        hirs3Sink.stop();
        hirs4Sink.stop();
        hirs5Sink.stop();
        hirs6Sink.stop();
        hirs7Sink.stop();
        hirs8Sink.stop();
        hirs9Sink.stop();
        hirs10Sink.stop();
        hirs11Sink.stop();
        hirs12Sink.stop();
        hirs13Sink.stop();
        hirs14Sink.stop();
        hirs15Sink.stop();
        hirs16Sink.stop();
        hirs17Sink.stop();
        hirs18Sink.stop();
        hirs19Sink.stop();
        hirs20Sink.stop();

        if (compositeThread.joinable()) {
            compositeThread.join();
        }

        compositeIn1.clearReadStop();
        compositeIn1.clearWriteStop();
        compositeIn2.clearReadStop();
        compositeIn2.clearWriteStop();
    };
    
    void setVFO(VFOManager::VFO* vfo) {
        _vfo = vfo;
        demod.setInput(_vfo->output);
    };

    virtual bool canRecord() {
        return false;
    }
    
    // bool startRecording(std::string recPath) {

    // };

    // void stopRecording() {

    // };

    // bool isRecording() {

    // };
    
    void drawMenu(float menuWidth) {
        ImGui::SetNextItemWidth(menuWidth);
        symDiag.draw();

        ImGui::Begin("NOAA HRPT Decoder");
        ImGui::BeginTabBar("NOAAHRPTTabs");

        if (ImGui::BeginTabItem("AVHRR RGB(221)")) {
            ImGui::BeginChild("AVHRRRGBChild");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
            avhrrRGBImage.draw();
            ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("AVHRR 1")) {
            ImGui::BeginChild("AVHRR1Child");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
            avhrr1Image.draw();
            ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("AVHRR 2")) {
            ImGui::BeginChild("AVHRR2Child");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
            avhrr2Image.draw();
            ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("AVHRR 3")) {
            ImGui::BeginChild("AVHRR3Child");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
            avhrr3Image.draw();
            ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("AVHRR 4")) {
            ImGui::BeginChild("AVHRR4Child");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
            avhrr4Image.draw();
            ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("AVHRR 5")) {
            ImGui::BeginChild("AVHRR5Child");
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
            avhrr5Image.draw();
            ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("HIRS")) {
            ImGui::BeginChild("HIRSChild");

            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
        ImGui::End();
    };
    
private:
    // AVHRR Data Handlers
    void avhrrCompositeWorker() {
        compositeIn1.flush();
        compositeIn2.flush();
        while (true) {
            if (compositeIn1.read() < 0) { return; }
            if (compositeIn2.read() < 0) { return; }
            
            uint8_t* buf = avhrrRGBImage.acquireNextLine();
            float rg, b;
            for (int i = 0; i < 2048; i++) {
                b = ((float)compositeIn1.readBuf[i] * 255.0f) / 1024.0f;
                rg = ((float)compositeIn2.readBuf[i] * 255.0f) / 1024.0f;
                buf[(i*4)] = rg;
                buf[(i*4) + 1] = rg;
                buf[(i*4) + 2] = b;
                buf[(i*4) + 3] = 255;
            }
            avhrrRGBImage.releaseNextLine();

            compositeIn1.flush();
            compositeIn2.flush();
        }
    }

    static void avhrr1Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;
        uint8_t* buf = _this->avhrr1Image.acquireNextLine();
        float val;
        for (int i = 0; i < 2048; i++) {
            val = ((float)data[i] * 255.0f) / 1024.0f;
            buf[(i*4)] = val;
            buf[(i*4) + 1] = val;
            buf[(i*4) + 2] = val;
            buf[(i*4) + 3] = 255;
        }
        _this->avhrr1Image.releaseNextLine();

        memcpy(_this->compositeIn1.writeBuf, data, count * sizeof(uint16_t));
        _this->compositeIn1.swap(count);
    }

    static void avhrr2Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;
        uint8_t* buf = _this->avhrr2Image.acquireNextLine();
        float val;
        for (int i = 0; i < 2048; i++) {
            val = ((float)data[i] * 255.0f) / 1024.0f;
            buf[(i*4)] = val;
            buf[(i*4) + 1] = val;
            buf[(i*4) + 2] = val;
            buf[(i*4) + 3] = 255;
        }
        _this->avhrr2Image.releaseNextLine();

        memcpy(_this->compositeIn2.writeBuf, data, count * sizeof(uint16_t));
        _this->compositeIn2.swap(count);
    }

    static void avhrr3Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;
        uint8_t* buf = _this->avhrr3Image.acquireNextLine();
        float val;
        for (int i = 0; i < 2048; i++) {
            val = ((float)data[i] * 255.0f) / 1024.0f;
            buf[(i*4)] = val;
            buf[(i*4) + 1] = val;
            buf[(i*4) + 2] = val;
            buf[(i*4) + 3] = 255;
        }
        _this->avhrr3Image.releaseNextLine();
    }

    static void avhrr4Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;
        uint8_t* buf = _this->avhrr4Image.acquireNextLine();
        float val;
        for (int i = 0; i < 2048; i++) {
            val = ((float)data[i] * 255.0f) / 1024.0f;
            buf[(i*4)] = val;
            buf[(i*4) + 1] = val;
            buf[(i*4) + 2] = val;
            buf[(i*4) + 3] = 255;
        }
        _this->avhrr4Image.releaseNextLine();
    }

    static void avhrr5Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;
        uint8_t* buf = _this->avhrr5Image.acquireNextLine();
        float val;
        for (int i = 0; i < 2048; i++) {
            val = ((float)data[i] * 255.0f) / 1024.0f;
            buf[(i*4)] = val;
            buf[(i*4) + 1] = val;
            buf[(i*4) + 2] = val;
            buf[(i*4) + 3] = 255;
        }
        _this->avhrr5Image.releaseNextLine();
    }

    // HIRS Data Handlers
    static void hirs1Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;

    }

    static void hirs2Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;

    }

    static void hirs3Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;

    }

    static void hirs4Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;

    }

    static void hirs5Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;

    }

    static void hirs6Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;

    }

    static void hirs7Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;

    }

    static void hirs8Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;

    }

    static void hirs9Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;

    }

    static void hirs10Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;

    }

    static void hirs11Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;

    }

    static void hirs12Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;

    }

    static void hirs13Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;

    }

    static void hirs14Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;

    }

    static void hirs15Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;

    }

    static void hirs16Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;

    }

    static void hirs17Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;

    }

    static void hirs18Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;

    }

    static void hirs19Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;

    }

    static void hirs20Handler(uint16_t* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;

    }

    static void visHandler(float* data, int count, void* ctx) {
        NOAAHRPTDecoder* _this = (NOAAHRPTDecoder*)ctx;
        memcpy(_this->symDiag.acquireBuffer(), data, 1024 * sizeof(float));
        _this->symDiag.releaseBuffer();
    }

    std::string _name;
    std::string _recPath;
    VFOManager::VFO* _vfo;

    // DSP
    dsp::PMDemod demod;

    dsp::stream<float> visStream;
    dsp::stream<float> dataStream;
    dsp::Splitter<float> split;

    dsp::Reshaper<float> reshape;

    dsp::ManchesterDeframer deframe;
    dsp::ManchesterDecoder manDec;
    dsp::BitPacker packer;
    dsp::noaa::HRPTDemux demux;
    dsp::noaa::TIPDemux tipDemux;
    dsp::noaa::HIRSDemux hirsDemux;

    // AHVRR Handlers
    dsp::HandlerSink<uint16_t> avhrr1Sink;
    dsp::HandlerSink<uint16_t> avhrr2Sink;
    dsp::HandlerSink<uint16_t> avhrr3Sink;
    dsp::HandlerSink<uint16_t> avhrr4Sink;
    dsp::HandlerSink<uint16_t> avhrr5Sink;

    // (at the moment) Unused TIP handlers
    dsp::NullSink<uint8_t> sbuvSink;
    dsp::NullSink<uint8_t> dcsSink;
    dsp::NullSink<uint8_t> semSink;

    // (at the moment) Unused AIP handlers
    dsp::NullSink<uint8_t> aipSink;

    // HIRS Handlers
    dsp::HandlerSink<uint16_t> hirs1Sink;
    dsp::HandlerSink<uint16_t> hirs2Sink;
    dsp::HandlerSink<uint16_t> hirs3Sink;
    dsp::HandlerSink<uint16_t> hirs4Sink;
    dsp::HandlerSink<uint16_t> hirs5Sink;
    dsp::HandlerSink<uint16_t> hirs6Sink;
    dsp::HandlerSink<uint16_t> hirs7Sink;
    dsp::HandlerSink<uint16_t> hirs8Sink;
    dsp::HandlerSink<uint16_t> hirs9Sink;
    dsp::HandlerSink<uint16_t> hirs10Sink;
    dsp::HandlerSink<uint16_t> hirs11Sink;
    dsp::HandlerSink<uint16_t> hirs12Sink;
    dsp::HandlerSink<uint16_t> hirs13Sink;
    dsp::HandlerSink<uint16_t> hirs14Sink;
    dsp::HandlerSink<uint16_t> hirs15Sink;
    dsp::HandlerSink<uint16_t> hirs16Sink;
    dsp::HandlerSink<uint16_t> hirs17Sink;
    dsp::HandlerSink<uint16_t> hirs18Sink;
    dsp::HandlerSink<uint16_t> hirs19Sink;
    dsp::HandlerSink<uint16_t> hirs20Sink;

    dsp::HandlerSink<float> visSink;

    ImGui::LinePushImage avhrrRGBImage;
    ImGui::LinePushImage avhrr1Image;
    ImGui::LinePushImage avhrr2Image;
    ImGui::LinePushImage avhrr3Image;
    ImGui::LinePushImage avhrr4Image;
    ImGui::LinePushImage avhrr5Image;

    ImGui::SymbolDiagram symDiag;

    dsp::stream<uint16_t> compositeIn1;
    dsp::stream<uint16_t> compositeIn2;
    std::thread compositeThread;

};