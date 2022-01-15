#include "devices.h"

Devices::Devices() {
    insert_dummy_device();
}

int Devices::add(Device&& device) {
    mDevices.emplace_back(device);
    return size();
}

Device& Devices::at(int index) {
    if (index > 0 && index < mDevices.size()) {
        return mDevices.at(index);
    }
    return mDevices.at(0); // empty dummy device, which is always present
}

int Devices::size() const {
    return mDevices.size() - 1;
}

bool Devices::empty() const {
    if (mDevices.size() > 1) {
        return false;
    }
    return true;
}

void Devices::clear() {
    mDevices.clear();
    insert_dummy_device();
}

Device& Devices::getDeviceBySerial(const std::string& serial) {
    auto isSerial = [&](const Device& device){ return device.serial() == serial; };

    auto it = std::find_if(std::begin(mDevices), std::end(mDevices), isSerial);
    return it != mDevices.end() ? *it : *(mDevices.begin());
}

void Devices::sortBySerial() {
    sort_devices_by_serial(mDevices);
}

Devices::iterator Devices::begin() {
    return mDevices.begin();
}

Devices::iterator Devices::end() {
    return mDevices.end();
}

Devices::const_iterator Devices::begin() const {
    return cbegin();
}

Devices::const_iterator Devices::end() const {
    return cend();
}

Devices::const_iterator Devices::cbegin() const {
    return mDevices.cbegin();
}

Devices::const_iterator Devices::cend() const {
    return mDevices.cend();
}

Devices::reverse_iterator Devices::rbegin() {
    return mDevices.rbegin();
}

Devices::reverse_iterator Devices::rend() {
    return mDevices.rend();
}

Devices::const_reverse_iterator Devices::crbegin() const {
    return mDevices.crbegin();
}

Devices::const_reverse_iterator Devices::crend() const {
    return mDevices.crend();
}

void Devices::insert_dummy_device() {
    mDevices.emplace_back(Device{});
    mDevices.back().product("None");
}

void Devices::sort_devices_by_serial(ContainerType& devices) {
    std::sort(devices.begin() + 1, devices.end(), [](const Device& lhs, const Device& rhs) {
        return lhs.serial() > rhs.serial();
    });
}
