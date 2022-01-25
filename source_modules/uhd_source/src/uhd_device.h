#pragma once

#include "device.h"

#include "dsp/stream.h"
#include "dsp/types.h"
#include "uhd_device.h"

#include <uhd/types/tune_request.hpp>
#include <uhd/usrp/multi_usrp.hpp>

#include <spdlog/spdlog.h>

#include <atomic>
#include <thread>

class UHDDevice {
public:
    UHDDevice(Device device) : device(std::move(device)) {
    }

    void open() {
        const uhd::device_addr_t args(device.toUhdArgs());

        if (!device.isValid()) {
            spdlog::warn("tried to open invalid device with serial {0}", device.serial());
            return;
        }

        usrp = uhd::usrp::multi_usrp::make(args);
        if (!usrp) {
            spdlog::error("could not make UHD device with serial {0}", device.serial());
            return;
        }

        const size_t rxChannelCount = getRxChannelCount();
        spdlog::debug("device has {0} rx channels", rxChannelCount);

        bandwidthRanges.clear();
        frequencyRanges.clear();
        gainRanges.clear();
        sampleRateRanges.clear();
        dcOffsetRanges.clear();
        bandwidthRanges.reserve(rxChannelCount);
        frequencyRanges.reserve(rxChannelCount);
        gainRanges.reserve(rxChannelCount);
        sampleRateRanges.reserve(rxChannelCount);
        dcOffsetRanges.reserve(rxChannelCount);

        for (size_t channelIndex = 0; channelIndex < rxChannelCount; ++channelIndex) {
            rxChannels.insert(channelIndex);
            bandwidthRanges.insert(bandwidthRanges.begin() + channelIndex, usrp->get_rx_bandwidth_range(channelIndex));
            frequencyRanges.insert(frequencyRanges.begin() + channelIndex, usrp->get_rx_freq_range(channelIndex));
            gainRanges.insert(gainRanges.begin() + channelIndex, usrp->get_rx_gain_range(channelIndex));
            sampleRateRanges.insert(sampleRateRanges.begin() + channelIndex, usrp->get_rx_rates(channelIndex));
            dcOffsetRanges.insert(dcOffsetRanges.begin() + channelIndex, usrp->get_rx_dc_offset_range(channelIndex));
            antennas = usrp->get_rx_antennas(currentChannelIndex);
            spdlog::info("bandwidth range from {0} to {1}", bandwidthRanges[channelIndex].start(), bandwidthRanges[channelIndex].stop());
            spdlog::info("frequency range from {0} to {1}", frequencyRanges[channelIndex].start(), frequencyRanges[channelIndex].stop());
            spdlog::info("gain range from {0} to {1}", gainRanges[channelIndex].start(), gainRanges[channelIndex].stop());
            spdlog::info("sample rate range from {0} to {1}", sampleRateRanges[channelIndex].start(), sampleRateRanges[channelIndex].stop());
            spdlog::info("dc offset range from {0} to {1}", dcOffsetRanges[channelIndex].start(), dcOffsetRanges[channelIndex].stop());
            spdlog::info("available antennas {0}", antennas.size());
            for (const auto& antenna : antennas) {
                spdlog::info("    {0}", antenna);
            }
        }

        const double rxRate = usrp->get_rx_rate(currentChannelIndex);
        const double rxFrequency = usrp->get_rx_freq(currentChannelIndex);
        const double rxBandwidth = usrp->get_rx_bandwidth(currentChannelIndex);
        const double rxGain = usrp->get_rx_gain(currentChannelIndex);
        spdlog::info("rx rate is {0} Sps", rxRate);
        spdlog::info("rx frequency is {0} Hz", rxFrequency);
        spdlog::info("rx bandwidth is {0} Hz", rxBandwidth);
        spdlog::info("rx gain is {0} dB", rxGain);
    }

    bool isOpen() const {
        return usrp ? true : false;
    }

    void startReceiving(dsp::stream<dsp::complex_t>& stream) {
        receiving = true;
        receivingThread = std::thread(&UHDDevice::worker, this, std::ref(stream));
    }

    void stopReceiving() {
        receiving = false;
        if (receivingThread.joinable()) {
            receivingThread.join();
        }
    }

    void close() {
        stopReceiving();
    }

    std::string serial() const {
        return device.serial();
    }

    void setChannelIndex(const size_t channel_index) { // [0..N-1]
        size_t rxChannels = 0;
        if (usrp) {
            rxChannels = getRxChannelCount();
            if ((channel_index >= 0) && (channel_index < rxChannels)) {
                currentChannelIndex = std::clamp<size_t>(channel_index, 0, rxChannels - 1);
            }
            else {
                spdlog::error("channel index {0} is not between 0 and {1}", channel_index, rxChannels - 1);
            }
        }
    }

    void setCenterFrequency(double center_frequency) { // [Hz]
        if (usrp) {
            center_frequency = std::clamp(center_frequency, getRxFrequencyMin(), getRxFrequencyMax());
            const uhd::tune_request_t tuneRequest(center_frequency);
            spdlog::debug("set rx center frequency to {0} Hz", center_frequency);
            uhd::tune_result_t result = usrp->set_rx_freq(tuneRequest, currentChannelIndex);
            spdlog::debug("tune result of setCenterFrequency: {0}", result.to_pp_string());
        }
    }

    void setRxBandwidth(double bandwidth) { // [Hz]
        if (usrp) {
            bandwidth = std::clamp(bandwidth, getRxBandwidthMin(), getRxBandwidthMax());
            spdlog::debug("set rx bandwidth to {0} Hz", bandwidth);
            usrp->set_rx_bandwidth(bandwidth, currentChannelIndex);
            spdlog::debug("actual bandwidth is {0} Hz", usrp->get_rx_bandwidth(currentChannelIndex));
        }
    }

    void setRxGain(double gain) { // [dB]
        if (usrp) {
            gain = std::clamp(gain, getRxGainMin(), getRxGainMax());
            spdlog::debug("set rx gain to {0} dB", gain);
            usrp->set_rx_gain(gain, currentChannelIndex);
        }
    }

    void setRxSampleRate(double sampleRate) { // [Sps]
        if (usrp) {
            sampleRate = std::clamp(sampleRate, getRxSampleRateMin(), getRxSampleRateMax());
            spdlog::debug("set rx sample rate to {0} Sps", sampleRate);
            usrp->set_rx_rate(sampleRate, currentChannelIndex);
        }
    }

    void setRxAntenna(const std::string& antenna) {
        if (usrp) {
            if (std::find(antennas.begin(), antennas.end(), antenna) != antennas.end()) {
                spdlog::debug("set rx antenna to {0}", antenna);
                usrp->set_rx_antenna(antenna, currentChannelIndex);
            }
        }
    }

    void setRxAntennaByIndex(const int antennaIndex) {
        if (usrp && (antennaIndex >= 0) && (antennaIndex < antennas.size())) {
            setRxAntenna(antennas[antennaIndex]);
        }
    }

    size_t getRxChannelCount() const { // [0..N-1]
        if (usrp) {
            return usrp->get_rx_num_channels();
        }
        return 0;
    }

    size_t getChannelIndex() const { // [0..N-1]
        return currentChannelIndex;
    }

    double getCenterFrequency() const { // [Hz]
        if (usrp) {
            return usrp->get_rx_freq(currentChannelIndex);
        }
        return 0.0;
    }

    double getRxBandwidth() const { // [Hz]
        if (usrp) {
            return usrp->get_rx_bandwidth(currentChannelIndex);
        }
        return 0.0;
    }

    double getRxGain() const { // [dB]
        if (usrp) {
            return usrp->get_rx_gain(currentChannelIndex);
        }
        return 0.0;
    }

    double getRxSampleRate() const { // [Sps]
        if (usrp) {
            return usrp->get_rx_rate(currentChannelIndex);
        }
        return 0.0;
    }

    std::vector<std::string> getAntennas() const {
        return antennas;
    }

    double getRxBandwidthMin() const { // [Hz]
        if (channelDataAvailable(bandwidthRanges)) {
            return bandwidthRanges[currentChannelIndex].start();
        }
        return 0.0;
    }

    double getRxBandwidthMax() const { // [Hz]
        if (channelDataAvailable(bandwidthRanges)) {
            return bandwidthRanges[currentChannelIndex].stop();
        }
        return 0.0;
    }

    double getRxFrequencyMin() const { // [Hz]
        if (channelDataAvailable(frequencyRanges)) {
            return frequencyRanges[currentChannelIndex].start();
        }
        return 0.0;
    }

    double getRxFrequencyMax() const { // [Hz]
        if (channelDataAvailable(frequencyRanges)) {
            return frequencyRanges[currentChannelIndex].stop();
        }
        return 0.0;
    }

    double getRxGainMin() const { // [dB]
        if (channelDataAvailable(gainRanges)) {
            return gainRanges[currentChannelIndex].start();
        }
        return 0.0;
    }

    double getRxGainMax() const { // [dB]
        if (channelDataAvailable(gainRanges)) {
            return gainRanges[currentChannelIndex].stop();
        }
        return 0.0;
    }

    double getRxSampleRateMin() const { // [Sps]
        if (channelDataAvailable(sampleRateRanges)) {
            return sampleRateRanges[currentChannelIndex].start();
        }
        return 0.0;
    }

    double getRxSampleRateMax() const { // [Sps]
        if (channelDataAvailable(sampleRateRanges)) {
            return sampleRateRanges[currentChannelIndex].stop();
        }
        return 0.0;
    }

    double getDcOffsetMin() const {
        if (channelDataAvailable(dcOffsetRanges)) {
            return dcOffsetRanges[currentChannelIndex].start();
        }
        return 0.0;
    }

    double getDcOffsetMax() const {
        if (channelDataAvailable(dcOffsetRanges)) {
            return dcOffsetRanges[currentChannelIndex].stop();
        }
        return 0.0;
    }

private:
    void worker(dsp::stream<dsp::complex_t>& stream) {
        if (!usrp) {
            spdlog::warn("tried to create worker thread on not yet opened device with serial {0}", device.serial());
            return;
        }
        // create a receive streamer
        uhd::stream_args_t stream_args("fc32"); // complex floats
        stream_args.channels = std::vector<size_t>{ currentChannelIndex };
        uhd::rx_streamer::sptr rxStream = usrp->get_rx_stream(stream_args);
        if (!rxStream) {
            return;
        }

        // setup streaming command
        uhd::stream_cmd_t stream_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
        stream_cmd.stream_now = true;
        rxStream->issue_stream_cmd(stream_cmd);

        // meta-data will be filled in by recv()
        uhd::rx_metadata_t md;
        // allocate buffer to receive with samples
        const size_t maxNumberOfSamples = std::min<size_t>(STREAM_BUFFER_SIZE, rxStream->get_max_num_samps());
        spdlog::debug("preparing buffer for up to {0} samples", maxNumberOfSamples);
        std::vector<std::complex<float>> buff(maxNumberOfSamples);
        std::vector<void*> buffs;
        for (size_t ch = 0; ch < rxStream->get_num_channels(); ch++)
            buffs.push_back(&buff.front()); // same buffer for each channel
        double receive_timeout = 1.5;

        while (receiving) {
            size_t numRxSamps = rxStream->recv(buffs, buff.size(), md, receive_timeout, false);

            receive_timeout = 0.1;

            // handle the error code
            if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_OVERFLOW) {
                spdlog::warn("{0}", md.strerror());
            }
            else if (md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE) {
                spdlog::error("error while receiving data: {0}", md.strerror());
                receiving = false;
            }

            for (int i = 0; i < numRxSamps; i++) {
                stream.writeBuf[i].re = buff[i].real();
                stream.writeBuf[i].im = buff[i].imag();
            }
            if (!stream.swap(numRxSamps)) { break; }
        }

        uhd::stream_cmd_t stream_cmd_finish(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
        rxStream->issue_stream_cmd(stream_cmd_finish);
    }

    bool channelDataAvailable(const std::vector<uhd::meta_range_t>& data) const {
        return currentChannelIndex < data.size();
    }

    size_t currentChannelIndex = 0;
    Device device;
    std::atomic<bool> receiving = false;
    std::thread receivingThread;
    uhd::usrp::multi_usrp::sptr usrp = nullptr;
    std::vector<uhd::meta_range_t> bandwidthRanges;  // bandwidth range per channel
    std::vector<uhd::meta_range_t> frequencyRanges;  // frequency range per channel
    std::vector<uhd::gain_range_t> gainRanges;       // gain range per channel
    std::vector<uhd::meta_range_t> sampleRateRanges; // sample rate range per channel
    std::vector<uhd::meta_range_t> dcOffsetRanges;   // dc offset range per channel
    std::vector<std::string> antennas;
    std::set<size_t> rxChannels;
};
