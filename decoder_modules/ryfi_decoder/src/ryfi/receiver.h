#pragma once
#include "utils/new_event.h"
#include "dsp/demod/psk.h"
#include "dsp/routing/doubler.h"
#include "packet.h"
#include "frame.h"
#include "rs_codec.h"
#include "conv_codec.h"
#include "framing.h"
#include <mutex>

namespace ryfi {
    class Receiver {
    public:
        Receiver();

        /**
         * Create a transmitter.
         * @param in Baseband input.
         * @param baudrate Baudrate to use over the air.
         * @param samplerate Samplerate of the baseband.
        */
        Receiver(dsp::stream<dsp::complex_t>* in, double baudrate, double samplerate);

        /**
         * Create a transmitter.
         * @param in Baseband input.
         * @param baudrate Baudrate to use over the air.
         * @param samplerate Samplerate of the baseband.
        */
        void init(dsp::stream<dsp::complex_t>* in, double baudrate, double samplerate);

        /**
         * Set the input stream.
         * @param in Baseband input.
        */
        void setInput(dsp::stream<dsp::complex_t>* in);

        // Destructor
        ~Receiver();

        /**
         * Start the transmitter's DSP.
        */
        void start();

        /**
         * Stop the transmitter's DSP.
        */
        void stop();
        
        dsp::stream<dsp::complex_t>* softOut;

        NewEvent<Packet> onPacket;

    private:
        void worker();

        // DSP
        dsp::demod::PSK<4> demod;
        dsp::routing::Doubler<dsp::complex_t> doubler;
        Deframer deframer;
        ConvDecoder conv;
        RSDecoder rs;

        bool running = false;
        std::thread workerThread;
    };
}