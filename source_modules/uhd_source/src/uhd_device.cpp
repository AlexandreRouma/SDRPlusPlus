#include "uhd_device.h"

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

    if (!mDevice.isValid()) {
        spdlog::warn("tried to open invalid device with serial {0}", mDevice.serial());
        return;
    }

    // create a usrp device
    mUsrp = uhd::usrp::multi_usrp::make(args);
    if (!mUsrp) {
        spdlog::error("could not make UHD device with serial {0}", mDevice.serial());
        return;
    }

    const size_t rxChannels = getRxChannelCount();
    spdlog::debug("device has {0} rx channels", rxChannels);

    mBandwidthRanges.clear();
    mFrequencyRanges.clear();
    mGainRanges.clear();
    mSampleRateRanges.clear();
    mDcOffsetRanges.clear();
    mBandwidthRanges.reserve(rxChannels);
    mFrequencyRanges.reserve(rxChannels);
    mGainRanges.reserve(rxChannels);
    mSampleRateRanges.reserve(rxChannels);
    mDcOffsetRanges.reserve(rxChannels);

    for (size_t channelIndex = 0; channelIndex < rxChannels; ++channelIndex) {
        mRxChannels.insert(channelIndex);
        mBandwidthRanges.insert(mBandwidthRanges.begin() + channelIndex, mUsrp->get_rx_bandwidth_range(channelIndex));
        mFrequencyRanges.insert(mFrequencyRanges.begin() + channelIndex, mUsrp->get_rx_freq_range(channelIndex));
        mGainRanges.insert(mGainRanges.begin() + channelIndex, mUsrp->get_rx_gain_range(channelIndex));
        mSampleRateRanges.insert(mSampleRateRanges.begin() + channelIndex, mUsrp->get_rx_rates(channelIndex));
        mDcOffsetRanges.insert(mDcOffsetRanges.begin() + channelIndex, mUsrp->get_rx_dc_offset_range(channelIndex));
        mAntennas = mUsrp->get_rx_antennas(mCurrentChannel);
        spdlog::info("bandwidth range from {0} to {1}", mBandwidthRanges[channelIndex].start(), mBandwidthRanges[channelIndex].stop());
        spdlog::info("frequency range from {0} to {1}", mFrequencyRanges[channelIndex].start(), mFrequencyRanges[channelIndex].stop());
        spdlog::info("gain range from {0} to {1}", mGainRanges[channelIndex].start(), mGainRanges[channelIndex].stop());
        spdlog::info("sample rate range from {0} to {1}", mSampleRateRanges[channelIndex].start(), mSampleRateRanges[channelIndex].stop());
        spdlog::info("dc offset range from {0} to {1}", mDcOffsetRanges[channelIndex].start(), mDcOffsetRanges[channelIndex].stop());
        spdlog::info("available antennas {0}", mAntennas.size());
        for (const auto& antenna : mAntennas) {
            spdlog::info("    {0}", antenna);
        }
    }

    const double rxRate = mUsrp->get_rx_rate(mCurrentChannel);
    const double rxFrequency = mUsrp->get_rx_freq(mCurrentChannel);
    const double rxBandwidth = mUsrp->get_rx_bandwidth(mCurrentChannel);
    const double rxGain = mUsrp->get_rx_gain(mCurrentChannel);
    spdlog::info("rx rate is {0} Sps", rxRate);
    spdlog::info("rx frequency is {0} Hz", rxFrequency);
    spdlog::info("rx bandwidth is {0} Hz", rxBandwidth);
    spdlog::info("rx gain is {0} dB", rxGain);
}

void UHDDevice::startReceiving(dsp::stream<dsp::complex_t>& stream) {
    mReceiving = true;
    mReceivingThread = std::thread(&UHDDevice::worker, this, std::ref(stream));
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

std::string UHDDevice::serial() const {
    return mDevice.serial();
}

void UHDDevice::setChannelIndex(const size_t channel_index) {
    size_t rxChannels = 0;
    if (mUsrp) {
        rxChannels = getRxChannelCount();
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
        spdlog::debug("set rx center frequency to {0} Hz", center_frequency);
        uhd::tune_result_t result = mUsrp->set_rx_freq(tuneRequest, mCurrentChannel);
        spdlog::debug("tune result of setCenterFrequency: {0}", result.to_pp_string());
    }
}

void UHDDevice::setRxBandwidth(double bandwidth) {
    if (mUsrp) {
        bandwidth = std::clamp(bandwidth, getRxBandwidthMin(), getRxBandwidthMax());
        spdlog::debug("set rx bandwidth to {0} Hz", bandwidth);
        mUsrp->set_rx_bandwidth(bandwidth, mCurrentChannel);
    }
}

void UHDDevice::setRxGain(double gain) {
    if (mUsrp) {
        gain = std::clamp(gain, getRxGainMin(), getRxGainMax());
        spdlog::debug("set rx gain to {0} dB", gain);
        mUsrp->set_rx_gain(gain, mCurrentChannel);
    }
}

void UHDDevice::setRxSampleRate(double sampleRate) {
    if (mUsrp) {
        sampleRate = std::clamp(sampleRate, getRxSampleRateMin(), getRxSampleRateMax());
        spdlog::debug("set rx sample rate to {0} Sps", sampleRate);
        mUsrp->set_rx_rate(sampleRate, mCurrentChannel);
    }
}

void UHDDevice::setRxAntenna(const std::string& antenna) {
    if (mUsrp) {
        if (std::find(mAntennas.begin(), mAntennas.end(), antenna) != mAntennas.end()) {
            spdlog::debug("set rx antenna to {0}", antenna);
            mUsrp->set_rx_antenna(antenna, mCurrentChannel);
        }
    }
}

void UHDDevice::setRxAntennaByIndex(const int antennaIndex) {
    if (mUsrp && (antennaIndex >= 0) && (antennaIndex < mAntennas.size())) {
        setRxAntenna(mAntennas[antennaIndex]);
    }
}

bool UHDDevice::isOpen() const {
    return mUsrp ? true : false;
}

size_t UHDDevice::getRxChannelCount() const {
    if (mUsrp) {
        return mUsrp->get_rx_num_channels();
    }
    return 0;
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

double UHDDevice::getRxSampleRate() const {
    if (mUsrp) {
        return mUsrp->get_rx_rate(mCurrentChannel);
    }
    return 0.0;
}

std::vector<std::string> UHDDevice::getAntennas() const {
    return mAntennas;
}

double UHDDevice::getRxBandwidthMin() const {
    if (channelDataAvailable(mBandwidthRanges)) {
        return mBandwidthRanges[mCurrentChannel].start();
    }
    return 0.0;
}

double UHDDevice::getRxBandwidthMax() const {
    if (channelDataAvailable(mBandwidthRanges)) {
        return mBandwidthRanges[mCurrentChannel].stop();
    }
    return 0.0;
}

double UHDDevice::getRxFrequencyMin() const {
    if (channelDataAvailable(mFrequencyRanges)) {
        return mFrequencyRanges[mCurrentChannel].start();
    }
    return 0.0;
}

double UHDDevice::getRxFrequencyMax() const {
    if (channelDataAvailable(mFrequencyRanges)) {
        return mFrequencyRanges[mCurrentChannel].stop();
    }
    return 0.0;
}

double UHDDevice::getRxGainMin() const {
    if (channelDataAvailable(mGainRanges)) {
        return mGainRanges[mCurrentChannel].start();
    }
    return 0.0;
}

double UHDDevice::getRxGainMax() const {
    if (channelDataAvailable(mGainRanges)) {
        return mGainRanges[mCurrentChannel].stop();
    }
    return 0.0;
}

double UHDDevice::getRxSampleRateMin() const {
    if (channelDataAvailable(mSampleRateRanges)) {
        return mSampleRateRanges[mCurrentChannel].start();
    }
    return 0.0;
}

double UHDDevice::getRxSampleRateMax() const {
    if (channelDataAvailable(mSampleRateRanges)) {
        return mSampleRateRanges[mCurrentChannel].stop();
    }
    return 0.0;
}

double UHDDevice::getDcOffsetMin() const {
    if (channelDataAvailable(mDcOffsetRanges)) {
        return mDcOffsetRanges[mCurrentChannel].start();
    }
    return 0.0;
}

double UHDDevice::getDcOffsetMax() const {
    if (channelDataAvailable(mDcOffsetRanges)) {
        return mDcOffsetRanges[mCurrentChannel].stop();
    }
    return 0.0;
}

void UHDDevice::worker(dsp::stream<dsp::complex_t>& stream) {
    if (!mUsrp) {
        spdlog::warn("tried to create worker thread on not yet opened device with serial {0}", mDevice.serial());
        return;
    }
    // create a receive streamer
    uhd::stream_args_t stream_args("fc32"); // complex floats
    stream_args.channels = std::vector<size_t>{ mCurrentChannel };
    mRxStream = mUsrp->get_rx_stream(stream_args);
    if (!mRxStream) {
        return;
    }

    // setup streaming command
    uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
    stream_cmd.stream_now = true;
    mRxStream->issue_stream_cmd(stream_cmd);

    // meta-data will be filled in by recv()
    uhd::rx_metadata_t md;
    // allocate buffer to receive with samples
    const size_t maxNumberOfSamples = std::min<size_t>(STREAM_BUFFER_SIZE, mRxStream->get_max_num_samps());
    spdlog::debug("preparing buffer for up to {0} samples", maxNumberOfSamples);
    std::vector<std::complex<float>> buff(maxNumberOfSamples);
    std::vector<void*> buffs;
    for (size_t ch = 0; ch < mRxStream->get_num_channels(); ch++)
        buffs.push_back(&buff.front()); // same buffer for each channel

    double receive_timeout = 1.5;
    while (mReceiving) {
        size_t num_rx_samps = mRxStream->recv(buffs, buff.size(), md, receive_timeout, false);
        // TODO: remove
        //spdlog::debug("{0} samples received", num_rx_samps);

        receive_timeout = 0.1;

        // handle the error code
        if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT)
            break;
        if (md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE) {
            spdlog::error("error while receiving data: {0}", md.strerror());
            mReceiving = false;
        }
        std::memcpy(stream.writeBuf, &buff.front(), maxNumberOfSamples);
        if (!stream.swap(maxNumberOfSamples)) { break; }
    }

    uhd::stream_cmd_t stream_cmd_finish(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
    mRxStream->issue_stream_cmd(stream_cmd_finish);
    mRxStream = nullptr;
}

bool UHDDevice::channelDataAvailable(const std::vector<uhd::meta_range_t>& data) const {
    if (mCurrentChannel < data.size()) {
        return true;
    }
    return false;
}
