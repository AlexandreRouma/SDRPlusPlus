#include "vor_receiver.h"
#include <dsp/buffer/reshaper.h>
#include <dsp/sink/handler_sink.h>
#include <utils/new_event.h>

namespace vor {
    // Note: hard coded to 22KHz samplerate
    class Decoder {
    public:
        /**
         * Create an instance of a VOR decoder.
         * @param in Input IQ stream at 22 KHz sampling rate.
         * @param integrationTime Integration time of the bearing data in seconds.
        */
        Decoder(dsp::stream<dsp::complex_t>* in, double integrationTime);

        // Destructor
        ~Decoder();

        /**
         * Set the input stream.
         * @param in Input IQ stream at 22 KHz sampling rate.
        */
        void setInput(dsp::stream<dsp::complex_t>* in);

        /**
         * Start the decoder.
        */
        void start();

        /**
         * Stop the decoder.
        */
        void stop();

        /**
         * handler(bearing, signalQuality);
        */
        NewEvent<float, float> onBearing;

    private:
        static void dataHandler(float* data, int count, void* ctx);

        // DSP
        Receiver rx;
        dsp::buffer::Reshaper<float> reshape;
        dsp::sink::Handler<float> symSink;
    };
}