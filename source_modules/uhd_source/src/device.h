#pragma once

#include <string>

class Device {
public:
    static const std::string NAME_FIELD;
    static const std::string PRODUCT_FIELD;
    static const std::string SERIAL_FIELD;
    static const std::string TYPE_FIELD;

    bool isValid() const {
        return !deviceSerial.empty();
    }

    std::string toUhdArgs() const {
        return std::string{ "serial=" + deviceSerial };
    }

    std::string name() const { // name: B205i
        return deviceName;
    }

    std::string product() const { // product: B205mini
        return productName;
    }

    std::string serial() const { // serial: 12A345B
        return deviceSerial;
    }

    std::string type() const { // type: b200
        return deviceType;
    }

    void name(std::string name) { // name: B205i
        deviceName = std::move(name);
    }

    void product(std::string product) { // product: B205mini
        productName = std::move(product);
    }

    void serial(std::string serial) { // serial: 12A345B
        deviceSerial = std::move(serial);
    }

    void type(std::string type) { // type: b200
        deviceType = std::move(type);
    }

private:
    std::string deviceName;
    std::string productName;
    std::string deviceSerial;
    std::string deviceType;
};
