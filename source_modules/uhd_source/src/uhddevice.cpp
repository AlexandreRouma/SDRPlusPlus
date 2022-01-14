#include "uhddevice.h"

#include <uhd/types/tune_request.hpp>
#include <spdlog/spdlog.h>

UHDDevice::UHDDevice(Device device)
    : mCurrentChannel(0), mReceiving(false), mDevice(std::move(device)), mUsrp(nullptr) {
}

UHDDevice::~UHDDevice() {
}

void UHDDevice::open() {
    const uhd::device_addr_t args(mDevice.to_uhd_args());
    const uhd::usrp::subdev_spec_t spec;

    // create a usrp device
    mUsrp = uhd::usrp::multi_usrp::make(args);
    if (!mUsrp) {
        spdlog::error("could not make UHD device with serial {0}", mDevice.serial());
        return;
    }

    const size_t rxChannels = mUsrp->get_rx_num_channels();
    spdlog::debug("device has {0} rx channels", rxChannels);

    mBandwidthRanges.clear();
    mFrequencyRanges.clear();
    mGainRanges.clear();
    mBandwidthRanges.reserve(rxChannels);
    mFrequencyRanges.reserve(rxChannels);
    mGainRanges.reserve(rxChannels);

    for (size_t channelIndex = 0; channelIndex < rxChannels; ++channelIndex) {
        mRxChannels.insert(channelIndex);
        mBandwidthRanges.insert(mBandwidthRanges.begin() + channelIndex, mUsrp->get_rx_bandwidth_range(channelIndex));
        mFrequencyRanges.insert(mFrequencyRanges.begin() + channelIndex, mUsrp->get_rx_freq_range(channelIndex));
        mGainRanges.insert(mGainRanges.begin() + channelIndex, mUsrp->get_rx_gain_range(channelIndex));
        spdlog::info("bandwidth range from {0} to {1}", mBandwidthRanges[channelIndex].start(), mBandwidthRanges[channelIndex].stop());
        spdlog::info("frequency range from {0} to {1}", mFrequencyRanges[channelIndex].start(), mFrequencyRanges[channelIndex].stop());
        spdlog::info("gain range from {0} to {1}", mGainRanges[channelIndex].start(), mGainRanges[channelIndex].stop());
    }

    const double rxRate = mUsrp->get_rx_rate(mCurrentChannel);
    const double rxFrequency = mUsrp->get_rx_freq(mCurrentChannel);
    const double rxBandwidth = mUsrp->get_rx_bandwidth(mCurrentChannel);
    const double rxGain = mUsrp->get_rx_gain(mCurrentChannel);
    spdlog::info("rx rate is {0}", rxRate);
    spdlog::info("rx frequency is {0}", rxFrequency);
    spdlog::info("rx bandwidth is {0}", rxBandwidth);
    spdlog::info("rx gain is {0}", rxGain);
}

void UHDDevice::startReceiving(dsp::stream<dsp::complex_t>& stream) {
    mReceiving = true;
    mReceivingThread = std::thread(&UHDDevice::_worker, this, std::ref(stream));
}

void UHDDevice::stopReceiving() {
    mReceiving = false;
    if (mReceivingThread.joinable()) {
        mReceivingThread.join();
    }
}

void UHDDevice::close() {
    stopReceiving();
}

void UHDDevice::setChannelIndex(const size_t channel_index) {
    size_t rxChannels = 0;
    if (mUsrp) {
        rxChannels = mUsrp->get_rx_num_channels();
        if ((channel_index >= 0) && (channel_index < rxChannels)) {
            mCurrentChannel = std::clamp<size_t>(channel_index, 0, rxChannels - 1);
        }
        else {
            spdlog::error("channel index {0} is not between 0 and {1}", channel_index, rxChannels - 1);
        }
    }
}

void UHDDevice::setCenterFrequency(double center_frequency) {
    if (mUsrp) {
        center_frequency = std::clamp(center_frequency, getRxFrequencyMin(), getRxFrequencyMax());
        const uhd::tune_request_t tuneRequest(center_frequency);
        spdlog::debug("set rx center frequency to {0}", center_frequency);
        uhd::tune_result_t result = mUsrp->set_rx_freq(tuneRequest, mCurrentChannel);
    }
}

void UHDDevice::setRxBandwidth(double bandwidth) {
    if (mUsrp) {
        bandwidth = std::clamp(bandwidth, getRxBandwidthMin(), getRxBandwidthMax());
        spdlog::debug("set rx bandwidth to {0}", bandwidth);
        mUsrp->set_rx_bandwidth(bandwidth, mCurrentChannel);
    }
}

void UHDDevice::setRxGain(double gain) {
    if (mUsrp) {
        gain = std::clamp(gain, getRxGainMin(), getRxGainMax());
        spdlog::debug("set rx gain to {0}", gain);
        mUsrp->set_rx_gain(gain, mCurrentChannel);
    }
}

bool UHDDevice::isOpen() const {
    return mUsrp ? true : false;
}

size_t UHDDevice::getChannelIndex() const {
    return mCurrentChannel;
}

double UHDDevice::getCenterFrequency() const {
    if (mUsrp) {
        return mUsrp->get_rx_freq(mCurrentChannel);
    }
    return 0.0;
}

double UHDDevice::getRxBandwidth() const {
    if (mUsrp) {
        return mUsrp->get_rx_bandwidth(mCurrentChannel);
    }
    return 0.0;
}

double UHDDevice::getRxGain() const {
    if (mUsrp) {
        return mUsrp->get_rx_gain(mCurrentChannel);
    }
    return 0.0;
}

double UHDDevice::getRxBandwidthMin() const {
    return mBandwidthRanges[mCurrentChannel].start();
}

double UHDDevice::getRxBandwidthMax() const {
    return mBandwidthRanges[mCurrentChannel].stop();
}

double UHDDevice::getRxFrequencyMin() const {
    return mFrequencyRanges[mCurrentChannel].start();
}

double UHDDevice::getRxFrequencyMax() const {
    return mFrequencyRanges[mCurrentChannel].stop();
}

double UHDDevice::getRxGainMin() const {
    return mGainRanges[mCurrentChannel].start();
}

double UHDDevice::getRxGainMax() const {
    return mGainRanges[mCurrentChannel].stop();
}

void UHDDevice::_worker(dsp::stream<dsp::complex_t>& stream) {
    // create a receive streamer
    uhd::stream_args_t stream_args("fc32"); // complex floats
    stream_args.channels = std::vector<size_t>{ mCurrentChannel };
    mRxStream = mUsrp->get_rx_stream(stream_args);
    if (!mRxStream) {
        mUsrp = nullptr;
        return;
    }

    // setup streaming command
    uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
    stream_cmd.stream_now = true;
    mRxStream->issue_stream_cmd(stream_cmd);

    // meta-data will be filled in by recv()
    uhd::rx_metadata_t md;
    // allocate buffer to receive with samples
    const size_t maxNumberOfSamples = mRxStream->get_max_num_samps();
    spdlog::debug("preparing buffer for up to {0} samples", maxNumberOfSamples);
    std::vector<std::complex<float>> buff(maxNumberOfSamples);
    std::vector<void*> buffs;
    for (size_t ch = 0; ch < mRxStream->get_num_channels(); ch++)
        buffs.push_back(&buff.front()); // same buffer for each channel

    double receive_timeout = 1.5;
    while (mReceiving) {
        size_t num_rx_samps = mRxStream->recv(buffs, buff.size(), md, receive_timeout, false);
        spdlog::debug("{0} samples received", num_rx_samps);

        receive_timeout = 0.1;

        // handle the error code
        if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT)
            break;
        if (md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE) {
            spdlog::error("error while receiving data: {0}", md.strerror());
            mReceiving = false;
        }
        const size_t numberOfSamples = std::min<size_t>(STREAM_BUFFER_SIZE, maxNumberOfSamples);
        std::memcpy(stream.writeBuf, &buff.front(), numberOfSamples);
        if (!stream.swap(numberOfSamples)) { break; }
    }

    uhd::stream_cmd_t stream_cmd_finish(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
    mRxStream->issue_stream_cmd(stream_cmd_finish);
    mRxStream = nullptr;
}
