#include "receiver.h"

#include "utils/flog.h"

namespace ryfi {
    Receiver::Receiver() {}

    Receiver::Receiver(dsp::stream<dsp::complex_t>* in, double baudrate, double samplerate) {
        init(in, baudrate, samplerate);
    }

    Receiver::~Receiver() {
        // Stop everything
        stop();
    }

    void Receiver::init(dsp::stream<dsp::complex_t>* in, double baudrate, double samplerate) {
        // Initialize the DSP
        demod.init(in, baudrate, samplerate, 31, 0.6, 0.1f, 0.005f, 1e-6, 0.01);
        doubler.init(&demod.out);
        softOut = &doubler.outA;
        deframer.setInput(&doubler.outB);
        conv.setInput(&deframer.out);
        rs.setInput(&conv.out);
    }

    void Receiver::setInput(dsp::stream<dsp::complex_t>* in) {
        demod.setInput(in);
    }

    void Receiver::start() {
        // Do nothing if already running
        if (running) { return; }

        // Start the worker thread
        workerThread = std::thread(&Receiver::worker, this);

        // Start the DSP
        demod.start();
        doubler.start();
        deframer.start();
        conv.start();
        rs.start();

        // Update the running state
        running = true;
    }

    void Receiver::stop() {
        // Do nothing if not running
        if (!running) { return; }

        // Stop the worker thread
        rs.out.stopReader();
        if (workerThread.joinable()) { workerThread.join(); }
        rs.out.clearReadStop();

        // Stop the DSP
        demod.stop();
        doubler.stop();
        deframer.stop();
        conv.stop();
        rs.stop();

        // Update the running state
        running = false;
    }
    
    void Receiver::worker() {
        Frame frame;
        uint16_t lastCounter = 0;
        uint8_t* pktBuffer = new uint8_t[Packet::MAX_CONTENT_SIZE];
        int pktExpected = 0;
        int pktRead = 0;
        int valid = 0;

        while (true) {
            // Read a frame
            int count = rs.out.read();
            if (count <= 0) { break; }

            // Deserialize the frame
            Frame::deserialize(rs.out.readBuf, frame);
            valid++;

            // Flush the stream
            rs.out.flush();

            //flog::info("Frame[{}]: FirstPacket={}, LastPacket={}", frame.counter, frame.firstPacket, frame.lastPacket);

            // Compute the expected frame counter
            uint16_t expectedCounter = lastCounter + 1;
            lastCounter = frame.counter;

            // If the frames aren't consecutive
            int frameRead = 0;
            if (frame.counter != expectedCounter) {
                flog::warn("Lost at least {} frames after {} valid frames", ((int)frame.counter - (int)expectedCounter + 0x10000) % 0x10000, valid);

                // Cancel the partial packet if there was one
                pktExpected = 0;
                pktRead = 0;
                valid = 1;

                // If this frame is not an idle frame or continuation frame
                if (frame.firstPacket != PKT_OFFS_NONE) {
                    // If the offset of the first packet is not plausible
                    if (frame.firstPacket > Frame::FRAME_DATA_SIZE-2) {
                        flog::warn("Packet had non-plausible offset: {}", frameRead);

                        // Skip the frame
                        continue;
                    }

                    // Skip to the end of the packet
                    frameRead = frame.firstPacket;
                }
            }

            // If there is no partial packet and the frame doesn't contain a packet start, skip it
            if (!pktExpected && frame.firstPacket == PKT_OFFS_NONE) { continue; }

            // Extract packets from the frame
            bool firstPacket = true;
            bool lastPacket = false;
            while (frameRead < Frame::FRAME_DATA_SIZE) {
                // If there is a partial packet read as much as possible from it
                if (pktExpected) {
                    // Compute how many bytes of the packet are available in the frame
                    int readable = std::min<int>(pktExpected - pktRead, Frame::FRAME_DATA_SIZE - frameRead);
                    //flog::debug("Reading {} bytes", readable);

                    // Write them to the packet
                    memcpy(&pktBuffer[pktRead], &frame.content[frameRead], readable);
                    pktRead += readable;
                    frameRead += readable;

                    // If the packet is read entirely
                    if (pktRead >= pktExpected) {
                        // Create the packet object
                        Packet pkt(pktBuffer, pktExpected);

                        // Send off the packet
                        onPacket(pkt);

                        // Prepare for the next packet
                        pktRead = 0;
                        pktExpected = 0;

                        // If this was the last packet of the frame
                        if (lastPacket || frame.firstPacket == PKT_OFFS_NONE) {
                            // Skip the rest of the frame
                            frameRead = Frame::FRAME_DATA_SIZE;
                            continue;
                        }
                    }

                    // Go to next packet
                    continue;
                }

                // If the packet offset is not plausible
                if (Frame::FRAME_DATA_SIZE - frameRead < 2) {
                    flog::warn("Packet had non-plausible offset: {}", frameRead);

                    // Skip the rest of the frame and the packet
                    frameRead = Frame::FRAME_DATA_SIZE;
                    pktExpected = 0;
                    pktRead = 0;
                    continue;
                }

                // If this is the first packet, use the frame info to skip possible left over data
                if (firstPacket) {
                    frameRead = frame.firstPacket;
                    firstPacket = false;
                }

                // Check if this is the last packet
                lastPacket = (frameRead == frame.lastPacket);

                // Parse the packet size
                pktExpected = ((uint16_t)frame.content[frameRead]) << 8;
                pktExpected |= (uint16_t)frame.content[frameRead+1];
                //flog::debug("Starting to read a {} byte packet at offset {}", pktExpected, frameRead);

                // Skip to the packet content
                frameRead += 2;
            }
        }

        delete[] pktBuffer;
    }
}