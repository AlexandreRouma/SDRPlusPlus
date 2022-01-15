#pragma once

#include "device.h"

#include "dsp/stream.h"
#include "dsp/types.h"

#include <uhd/types/tune_request.hpp>
#include <uhd/usrp/multi_usrp.hpp>

#include <atomic>
#include <thread>

class UHDDevice {
public:
    UHDDevice(Device device);
    ~UHDDevice();

    void open();
    bool isOpen() const;
    void startReceiving(dsp::stream<dsp::complex_t>& stream);
    void stopReceiving();
    void close();

    std::string serial() const;

    void setChannelIndex(const size_t channel_index); // [0..N-1]
    void setCenterFrequency(double center_frequency); // [Hz]
    void setRxBandwidth(double bandwidth);            // [Hz]
    void setRxGain(double gain);                      // [dB]
    void setRxSampleRate(double sampleRate);          // [Sps]
    void setRxAntenna(const std::string& antenna);
    void setRxAntennaByIndex(const int antennaIndex);

    size_t getRxChannelCount() const;  // [0..N-1]
    size_t getChannelIndex() const;    // [0..N-1]
    double getCenterFrequency() const; // [Hz]
    double getRxBandwidth() const;     // [Hz]
    double getRxGain() const;          // [dB]
    double getRxSampleRate() const;    // [Sps]
    std::vector<std::string> getAntennas() const;

    double getRxBandwidthMin() const;  // [Hz]
    double getRxBandwidthMax() const;  // [Hz]
    double getRxFrequencyMin() const;  // [Hz]
    double getRxFrequencyMax() const;  // [Hz]
    double getRxGainMin() const;       // [dB]
    double getRxGainMax() const;       // [dB]
    double getRxSampleRateMin() const; // [Sps]
    double getRxSampleRateMax() const; // [Sps]

    void worker(dsp::stream<dsp::complex_t>& stream);

private:
    bool channelDataAvailable(const std::vector<uhd::meta_range_t>& data) const;

    size_t mCurrentChannel;
    std::atomic<bool> mReceiving;
    std::thread mReceivingThread;
    Device mDevice;
    uhd::usrp::multi_usrp::sptr mUsrp;
    uhd::rx_streamer::sptr mRxStream;
    std::vector<uhd::meta_range_t> mBandwidthRanges;  // bandwidth range per channel
    std::vector<uhd::meta_range_t> mFrequencyRanges;  // frequency range per channel
    std::vector<uhd::gain_range_t> mGainRanges;       // gain range per channel
    std::vector<uhd::meta_range_t> mSampleRateRanges; // bandwidth range per channel
    std::vector<std::string> mAntennas;
    std::set<size_t> mRxChannels;
};
