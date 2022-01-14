#ifndef UHDDEVICE_H
#define UHDDEVICE_H

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

    void setChannelIndex(const size_t channel_index);
    void setCenterFrequency(double center_frequency);
    void setRxBandwidth(double bandwidth);
    void setRxGain(double gain);

    size_t getChannelIndex() const;
    double getCenterFrequency() const;
    double getRxBandwidth() const;
    double getRxGain() const;

    double getRxBandwidthMin() const;
    double getRxBandwidthMax() const;
    double getRxFrequencyMin() const;
    double getRxFrequencyMax() const;
    double getRxGainMin() const;
    double getRxGainMax() const;

    void _worker(dsp::stream<dsp::complex_t>& stream);

private:
    size_t mCurrentChannel;
    std::atomic<bool> mReceiving;
    std::thread mReceivingThread;
    Device mDevice;
    uhd::usrp::multi_usrp::sptr mUsrp;
    uhd::rx_streamer::sptr mRxStream;
    std::vector<uhd::meta_range_t> mBandwidthRanges; // bandwidth range per channel
    std::vector<uhd::meta_range_t> mFrequencyRanges; // frequency range per channel
    std::vector<uhd::gain_range_t> mGainRanges;      // gain range per channel
    std::set<size_t> mRxChannels;
};

#endif // UHDDEVICE_H
