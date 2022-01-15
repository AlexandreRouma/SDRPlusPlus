#pragma once

#include <string>

class Device {
public:
    static const std::string NAME_FIELD;
    static const std::string PRODUCT_FIELD;
    static const std::string SERIAL_FIELD;
    static const std::string TYPE_FIELD;

    bool isValid() const;

    std::string to_uhd_args() const;

    std::string name() const;    // name: B205i
    std::string product() const; // product: B205mini
    std::string serial() const;  // serial: 12A345B
    std::string type() const;    // type: b200

    void name(std::string name);       // name: B205i
    void product(std::string product); // product: B205mini
    void serial(std::string serial);   // serial: 12A345B
    void type(std::string type);       // type: b200

private:
    std::string mName;
    std::string mProduct;
    std::string mSerial;
    std::string mType;
};
