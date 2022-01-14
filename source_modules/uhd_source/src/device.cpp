#include "device.h"

const std::string Device::NAME_FIELD = "name";
const std::string Device::PRODUCT_FIELD = "product";
const std::string Device::SERIAL_FIELD = "serial";
const std::string Device::TYPE_FIELD = "type";

std::string Device::to_uhd_args() const {
    return std::string{ "serial=" + mSerial };
}

std::string Device::name() const {
    return mName;
}

std::string Device::product() const {
    return mProduct;
}

std::string Device::serial() const {
    return mSerial;
}

std::string Device::type() const {
    return mType;
}

void Device::name(std::string name) {
    mName = std::move(name);
}

void Device::product(std::string product) {
    mProduct = std::move(product);
}

void Device::serial(std::string serial) {
    mSerial = std::move(serial);
}

void Device::type(std::string type) {
    mType = std::move(type);
}
