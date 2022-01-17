#pragma once

#include <string>

class Device {
public:
    static const std::string NAME_FIELD;
    static const std::string PRODUCT_FIELD;
    static const std::string SERIAL_FIELD;
    static const std::string TYPE_FIELD;

    bool isValid() const {
        return !mSerial.empty();
    }

    std::string toUhdArgs() const {
        return std::string{ "serial=" + mSerial };
    }

    std::string getName() const { // name: B205i
        return mName;
    }

    std::string getProduct() const { // product: B205mini
        return mProduct;
    }

    std::string getSerial() const { // serial: 12A345B
        return mSerial;
    }

    std::string getType() const { // type: b200
        return mType;
    }

    void setName(std::string name) { // name: B205i
        mName = std::move(name);
    }

    void setProduct(std::string product) { // product: B205mini
        mProduct = std::move(product);
    }

    void setSerial(std::string serial) { // serial: 12A345B
        mSerial = std::move(serial);
    }

    void setType(std::string type) { // type: b200
        mType = std::move(type);
    }

private:
    std::string mName;
    std::string mProduct;
    std::string mSerial;
    std::string mType;
};
