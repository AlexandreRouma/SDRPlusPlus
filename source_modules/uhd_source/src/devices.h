#pragma once

#include "device.h"

#include <algorithm>
#include <vector>

class Devices {
public:
    using ContainerType = std::vector<Device>;

    Devices() {
        insertDummyDevice();
    }

    // returns the index at which the device was inserted
    int add(Device&& device) {
        devices.emplace_back(device);
        return size();
    }

    Device& at(int index) {
        if (index > 0 && index < devices.size()) {
            return devices.at(index);
        }
        return devices.at(0); // empty dummy device, which is always present
    }

    int size() const {
        return devices.size() - 1;
    }

    bool empty() const {
        return devices.size() <= 1;
    }

    void clear() {
        devices.clear();
        insertDummyDevice();
    }

    Device& getDeviceBySerial(const std::string& serial) {
        auto isSerial = [&](const Device& device) { return device.serial() == serial; };

        auto it = std::find_if(std::begin(devices), std::end(devices), isSerial);
        return it != devices.end() ? *it : *(devices.begin());
    }

    int getDeviceIndexBySerial(const std::string& serial) {
        auto isSerial = [&](const Device& device) { return device.serial() == serial; };

        auto it = std::find_if(std::begin(devices), std::end(devices), isSerial);
        if (it != devices.end()) {
            return static_cast<int>(std::distance(devices.begin(), it));
        }
        return 0;
    }

    void sortBySerial() {
        sortDevicesBySerial(devices);
    }

    using iterator = ContainerType::iterator;
    using const_iterator = ContainerType::const_iterator;
    using reverse_iterator = ContainerType::reverse_iterator;
    using const_reverse_iterator = ContainerType::const_reverse_iterator;

    iterator begin() {
        return devices.begin();
    }

    iterator end() {
        return devices.end();
    }

    const_iterator begin() const {
        return cbegin();
    }

    const_iterator end() const {
        return cend();
    }

    const_iterator cbegin() const {
        return devices.cbegin();
    }

    const_iterator cend() const {
        return devices.cend();
    }

    reverse_iterator rbegin() {
        return devices.rbegin();
    }

    reverse_iterator rend() {
        return devices.rend();
    }

    const_reverse_iterator crbegin() const {
        return devices.crbegin();
    }

    const_reverse_iterator crend() const {
        return devices.crend();
    }

private:
    void insertDummyDevice() {
        devices.emplace_back(Device{});
        devices.back().product("None");
    }

    static void sortDevicesBySerial(ContainerType& devicesToSort) {
        std::sort(devicesToSort.begin() + 1, devicesToSort.end(), [](const Device& lhs, const Device& rhs) {
            return lhs.serial() > rhs.serial();
        });
    }

    ContainerType devices;
};
