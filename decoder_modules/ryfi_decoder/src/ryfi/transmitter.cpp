#include "transmitter.h"

namespace ryfi {
    Transmitter::Transmitter(double baudrate, double samplerate) {
        // Initialize the DSP
        rs.setInput(&in);
        conv.setInput(&rs.out);
        framer.setInput(&conv.out);
        resamp.init(&framer.out, baudrate, samplerate);

        rrcTaps = dsp::taps::rootRaisedCosine<float>(511, 0.6, baudrate, samplerate);
        // Normalize the taps
        float tot = 0.0f;
        for (int i = 0; i < rrcTaps.size; i++) {
            tot += rrcTaps.taps[i];
        }
        for (int i = 0; i < rrcTaps.size; i++) {
            rrcTaps.taps[i] /= tot;
        }

        rrc.init(&resamp.out, rrcTaps);
        out = &rrc.out;
    }

    Transmitter::~Transmitter() {
        // Stop everything
        stop();
    }

    void Transmitter::start() {
        // Do nothing if already running
        if (running) { return; }

        // Start the worker thread
        workerThread = std::thread(&Transmitter::worker, this);

        // Start the DSP
        rs.start();
        conv.start();
        framer.start();
        resamp.start();
        rrc.start();

        // Update the running state
        running = true;
    }

    void Transmitter::stop() {
        // Do nothing if not running
        if (!running) { return; }

        // Stop the worker thread
        in.stopWriter();
        if (workerThread.joinable()) { workerThread.join(); }
        in.clearWriteStop();

        // Stop the DSP
        rs.stop();
        conv.stop();
        framer.stop();
        resamp.stop();
        rrc.stop();

        // Update the running state
        running = false;
    }

    bool Transmitter::send(const Packet& pkt) {
        // Acquire the packet queue
        std::lock_guard<std::mutex> lck(packetsMtx);

        // If there are too many packets queued up, drop the packet
        if (packets.size() >= MAX_QUEUE_SIZE) { return false; }

        // Push the packet onto the queue
        packets.push(pkt);
    }

    bool Transmitter::txFrame(const Frame& frame) {
        // Serialize the frame
        int count = frame.serialize(in.writeBuf);

        // Send it off
        return in.swap(count);
    }

    Packet Transmitter::popPacket() {
        // Acquire the packet queue
        std::unique_lock<std::mutex> lck(packetsMtx);

        // If no packets are available, return empty packet
        if (!packets.size()) { return Packet(); }

        // Pop the front packet and return it
        Packet pkt = packets.front();
        packets.pop();
        return pkt;
    }

    void Transmitter::worker() {
        Frame frame;
        Packet pkt;
        uint16_t counter = 0;
        int pktToWrite = 0;
        int pktWritten = 0;
        uint8_t* pktBuffer = new uint8_t[Packet::MAX_SERIALIZED_SIZE];

        while (true) {
            // Initialize the frame
            frame.counter = counter++;
            frame.firstPacket = PKT_OFFS_NONE;
            frame.lastPacket = PKT_OFFS_NONE;
            int frameOffset = 0;

            // Fill the frame with as much packet data as possible
            while (frameOffset < sizeof(Frame::content)) {
                // If there is no packet in the process of being sent
                if (!pktWritten) {
                    // If there is not enough space for the size of the packet
                    if ((sizeof(Frame::content) - frameOffset) < 2) {
                        // Fill the rest of the frame with noise and send it
                        for (int i = frameOffset; i < sizeof(Frame::content); i++) { frame.content[i] = rand(); }
                        break;
                    }

                    // Get the next packet
                    pkt = popPacket();

                    // If there was an available packet
                    if (pkt) {
                        // Serialize the packet
                        pktToWrite = pkt.serializedSize();
                        pkt.serialize(pktBuffer);
                    }
                }

                // If none was available
                if (!pkt) {
                    // Fill the rest of the frame with noise and send it
                    for (int i = frameOffset; i < sizeof(Frame::content); i++) { frame.content[i] = rand(); }
                    break;
                }

                // If this is the beginning of the packet
                if (!pktWritten) {
                    //flog::debug("Starting to write a {} byte packet at offset {}", pktToWrite-2, frameOffset);

                    // If this is the first packet of the frame, update its offset
                    if (frame.firstPacket == PKT_OFFS_NONE) { frame.firstPacket = frameOffset; }

                    // Update the last packet pointer
                    frame.lastPacket = frameOffset;
                }

                // Compute the amount of data writeable to the frame
                int writeable = std::min<int>(pktToWrite - pktWritten, sizeof(Frame::content) - frameOffset);

                // Copy the data to the frame
                memcpy(&frame.content[frameOffset], &pktBuffer[pktWritten], writeable);
                pktWritten += writeable;
                frameOffset += writeable;

                // If the packet is done being sent
                if (pktWritten >= pktToWrite) {
                    // Prepare for a new packet
                    pktToWrite = 0;
                    pktWritten = 0;
                }
            }

            // Send the frame
            if (!txFrame(frame)) { break; }
        }

        delete[] pktBuffer;
    }
}