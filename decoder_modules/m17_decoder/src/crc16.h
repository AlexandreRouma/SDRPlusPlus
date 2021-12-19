// Copyright 2020 Mobilinkd LLC.

#pragma once

#include <cstdint>
#include <array>
#include <cstddef>

namespace mobilinkd {

    template <uint16_t Poly = 0x5935, uint16_t Init = 0xFFFF>
    struct CRC16 {
        static constexpr uint16_t MASK = 0xFFFF;
        static constexpr uint16_t LSB = 0x0001;
        static constexpr uint16_t MSB = 0x8000;

        uint16_t reg_ = Init;

        void reset() {
            reg_ = Init;

            for (size_t i = 0; i != 16; ++i) {
                auto bit = reg_ & LSB;
                if (bit) reg_ ^= Poly;
                reg_ >>= 1;
                if (bit) reg_ |= MSB;
            }

            reg_ &= MASK;
        }

        void operator()(uint8_t byte) {
            reg_ = crc(byte, reg_);
        }

        uint16_t crc(uint8_t byte, uint16_t reg) {
            for (size_t i = 0; i != 8; ++i) {
                auto msb = reg & MSB;
                reg = ((reg << 1) & MASK) | ((byte >> (7 - i)) & LSB);
                if (msb) reg ^= Poly;
            }
            return reg & MASK;
        }

        uint16_t get() {
            auto reg = reg_;
            for (size_t i = 0; i != 16; ++i) {
                auto msb = reg & MSB;
                reg = ((reg << 1) & MASK);
                if (msb) reg ^= Poly;
            }
            return reg;
        }

        std::array<uint8_t, 2> get_bytes() {
            auto crc = get();
            std::array<uint8_t, 2> result{ uint8_t((crc >> 8) & 0xFF), uint8_t(crc & 0xFF) };
            return result;
        }
    };

} // mobilinkd