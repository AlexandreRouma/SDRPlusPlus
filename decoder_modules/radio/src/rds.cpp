#include "rds.h"
#include <string.h>
#include <map>
#include <algorithm>

namespace rds {
    std::map<uint16_t, BlockType> SYNDROMES = {
        { 0b1111011000, BLOCK_TYPE_A  },
        { 0b1111010100, BLOCK_TYPE_B  },
        { 0b1001011100, BLOCK_TYPE_C  },
        { 0b1111001100, BLOCK_TYPE_CP },
        { 0b1001011000, BLOCK_TYPE_D  }
    };

    std::map<BlockType, uint16_t> OFFSETS = {
        { BLOCK_TYPE_A,  0b0011111100 },
        { BLOCK_TYPE_B,  0b0110011000 },
        { BLOCK_TYPE_C,  0b0101101000 },
        { BLOCK_TYPE_CP, 0b1101010000 },
        { BLOCK_TYPE_D,  0b0110110100 }
    };
    
    std::map<uint8_t, std::string> EBUtoUTF8Table = { // Conversion map from Complete EBU Latin based repertoire to UTF-8
        {0x20, "\u0020"},
        {0x21, "\u0021"},
        {0x22, "\u0022"},
        {0x23, "\u0023"},
        {0x24, "\u00a4"},
        {0x25, "\u0025"},
        {0x26, "\u0026"},
        {0x27, "\u0027"},
        {0x28, "\u0028"},
        {0x29, "\u0029"},
        {0x2a, "\u002a"},
        {0x2b, "\u002b"},
        {0x2c, "\u002c"},
        {0x2d, "\u002d"},
        {0x2e, "\u002e"},
        {0x2f, "\u002f"},
        {0x30, "\u0030"},
        {0x31, "\u0031"},
        {0x32, "\u0032"},
        {0x33, "\u0033"},
        {0x34, "\u0034"},
        {0x35, "\u0035"},
        {0x36, "\u0036"},
        {0x37, "\u0037"},
        {0x38, "\u0038"},
        {0x39, "\u0039"},
        {0x3a, "\u003a"},
        {0x3b, "\u003b"},
        {0x3c, "\u003c"},
        {0x3d, "\u003d"},
        {0x3e, "\u003e"},
        {0x3f, "\u003f"},
        {0x40, "\u0040"},
        {0x41, "\u0041"},
        {0x42, "\u0042"},
        {0x43, "\u0043"},
        {0x44, "\u0044"},
        {0x45, "\u0045"},
        {0x46, "\u0046"},
        {0x47, "\u0047"},
        {0x48, "\u0048"},
        {0x49, "\u0049"},
        {0x4a, "\u004a"},
        {0x4b, "\u004b"},
        {0x4c, "\u004c"},
        {0x4d, "\u004d"},
        {0x4e, "\u004e"},
        {0x4f, "\u004f"},
        {0x50, "\u0050"},
        {0x51, "\u0051"},
        {0x52, "\u0052"},
        {0x53, "\u0053"},
        {0x54, "\u0054"},
        {0x55, "\u0055"},
        {0x56, "\u0056"},
        {0x57, "\u0057"},
        {0x58, "\u0058"},
        {0x59, "\u0059"},
        {0x5a, "\u005a"},
        {0x5b, "\u005b"},
        {0x5c, "\u005c"},
        {0x5d, "\u005d"},
        {0x5e, "\u2015"},
        {0x5f, "\u005f"},
        {0x60, "\u2551"},
        {0x61, "\u0061"},
        {0x62, "\u0062"},
        {0x63, "\u0063"},
        {0x64, "\u0064"},
        {0x65, "\u0065"},
        {0x66, "\u0066"},
        {0x67, "\u0067"},
        {0x68, "\u0068"},
        {0x69, "\u0069"},
        {0x6a, "\u006a"},
        {0x6b, "\u006b"},
        {0x6c, "\u006c"},
        {0x6d, "\u006d"},
        {0x6e, "\u006e"},
        {0x6f, "\u006f"},
        {0x70, "\u0070"},
        {0x71, "\u0071"},
        {0x72, "\u0072"},
        {0x73, "\u0073"},
        {0x74, "\u0074"},
        {0x75, "\u0075"},
        {0x76, "\u0076"},
        {0x77, "\u0077"},
        {0x78, "\u0078"},
        {0x79, "\u0079"},
        {0x7a, "\u007a"},
        {0x7b, "\u007b"},
        {0x7c, "\u007c"},
        {0x7d, "\u007d"},
        {0x7e, "\u00af"},
        {0x80, "\u00e1"},
        {0x81, "\u00e0"},
        {0x82, "\u00e9"},
        {0x83, "\u00e8"},
        {0x84, "\u00ed"},
        {0x85, "\u00ec"},
        {0x86, "\u00f3"},
        {0x87, "\u00f2"},
        {0x88, "\u00fa"},
        {0x89, "\u00f9"},
        {0x8a, "\u00d1"},
        {0x8b, "\u00c7"},
        {0x8c, "\u015e"},
        {0x8d, "\u00df"},
        {0x8e, "\u00a1"},
        {0x8f, "\u0132"},
        {0x90, "\u00e2"},
        {0x91, "\u00e4"},
        {0x92, "\u00ea"},
        {0x93, "\u00eb"},
        {0x94, "\u00ee"},
        {0x95, "\u00ef"},
        {0x96, "\u00f4"},
        {0x97, "\u00f6"},
        {0x98, "\u00fb"},
        {0x99, "\u00fc"},
        {0x9a, "\u00f1"},
        {0x9b, "\u00e7"},
        {0x9c, "\u015f"},
        {0x9d, "\u011f"},
        {0x9e, "\u0131"},
        {0x9f, "\u0133"},
        {0xa0, "\u00aa"},
        {0xa1, "\u03b1"},
        {0xa2, "\u00a9"},
        {0xa3, "\u2030"},
        {0xa4, "\u011e"},
        {0xa5, "\u011b"},
        {0xa6, "\u0148"},
        {0xa7, "\u0151"},
        {0xa8, "\u03c0"},
        {0xa9, "\u20ac"},
        {0xaa, "\u00a3"},
        {0xab, "\u0024"},
        {0xac, "\u2190"},
        {0xad, "\u2191"},
        {0xae, "\u2192"},
        {0xaf, "\u2193"},
        {0xb0, "\u00ba"},
        {0xb1, "\u00b9"},
        {0xb2, "\u00b2"},
        {0xb3, "\u00b3"},
        {0xb4, "\u00b1"},
        {0xb5, "\u0130"},
        {0xb6, "\u0144"},
        {0xb7, "\u0171"},
        {0xb8, "\u00b5"},
        {0xb9, "\u00bf"},
        {0xba, "\u00f7"},
        {0xbb, "\u00b0"},
        {0xbc, "\u00bc"},
        {0xbd, "\u00bd"},
        {0xbe, "\u00be"},
        {0xbf, "\u00a7"},
        {0xc0, "\u00c1"},
        {0xc1, "\u00c0"},
        {0xc2, "\u00c9"},
        {0xc3, "\u00c8"},
        {0xc4, "\u00cd"},
        {0xc5, "\u00cc"},
        {0xc6, "\u00d3"},
        {0xc7, "\u00d2"},
        {0xc8, "\u00da"},
        {0xc9, "\u00d9"},
        {0xca, "\u0158"},
        {0xcb, "\u010c"},
        {0xcc, "\u0160"},
        {0xcd, "\u017d"},
        {0xce, "\u00d0"},
        {0xcf, "\u013f"},
        {0xd0, "\u00c2"},
        {0xd1, "\u00c4"},
        {0xd2, "\u00ca"},
        {0xd3, "\u00cb"},
        {0xd4, "\u00ce"},
        {0xd5, "\u00cf"},
        {0xd6, "\u00d4"},
        {0xd7, "\u00d6"},
        {0xd8, "\u00db"},
        {0xd9, "\u00dc"},
        {0xda, "\u0159"},
        {0xdb, "\u010d"},
        {0xdc, "\u0161"},
        {0xdd, "\u017e"},
        {0xde, "\u0111"},
        {0xdf, "\u0140"},
        {0xe0, "\u00c3"},
        {0xe1, "\u00c5"},
        {0xe2, "\u00c6"},
        {0xe3, "\u0152"},
        {0xe4, "\u0177"},
        {0xe5, "\u00dd"},
        {0xe6, "\u00d5"},
        {0xe7, "\u00d8"},
        {0xe8, "\u00de"},
        {0xe9, "\u014a"},
        {0xea, "\u0154"},
        {0xeb, "\u0106"},
        {0xec, "\u015a"},
        {0xed, "\u0179"},
        {0xee, "\u0166"},
        {0xef, "\u00f0"},
        {0xf0, "\u00e3"},
        {0xf1, "\u00e5"},
        {0xf2, "\u00e6"},
        {0xf3, "\u0153"},
        {0xf4, "\u0175"},
        {0xf5, "\u00fd"},
        {0xf6, "\u00f5"},
        {0xf7, "\u00f8"},
        {0xf8, "\u00fe"},
        {0xf9, "\u014b"},
        {0xfa, "\u0155"},
        {0xfb, "\u0107"},
        {0xfc, "\u015b"},
        {0xfd, "\u017a"},
        {0xfe, "\u0167"}
    };

    //                           9876543210
    const uint16_t LFSR_POLY = 0b0110111001;
    const uint16_t IN_POLY   = 0b1100011011;

    const int BLOCK_LEN = 26;
    const int DATA_LEN = 16;
    const int POLY_LEN = 10;

    void RDSDecoder::process(uint8_t* symbols, int count) {
        for (int i = 0; i < count; i++) {
            // Shift in the bit
            shiftReg = ((shiftReg << 1) & 0x3FFFFFF) | (symbols[i] & 1);

            // Skip if we need to shift in new data
            if (--skip > 0) { continue; }

            // Calculate the syndrome and update sync status
            uint16_t syn = calcSyndrome(shiftReg);
            auto synIt = SYNDROMES.find(syn);
            bool knownSyndrome = synIt != SYNDROMES.end();
            sync = std::clamp<int>(knownSyndrome ? ++sync : --sync, 0, 4);
            
            // If we're still no longer in sync, try to resync
            if (!sync) { continue; }

            // Figure out which block we've got
            BlockType type;
            if (knownSyndrome) {
                type = SYNDROMES[syn];
            }
            else {
                type = (BlockType)((lastType + 1) % _BLOCK_TYPE_COUNT);
            }

            // Save block while correcting errors (NOT YET)
            blocks[type] = correctErrors(shiftReg, type, blockAvail[type]);

            // Update continous group count
            if (type == BLOCK_TYPE_A) { contGroup = 1; }
            else if (type == BLOCK_TYPE_B && lastType == BLOCK_TYPE_A) { contGroup++; }
            else if ((type == BLOCK_TYPE_C || type == BLOCK_TYPE_CP) && lastType == BLOCK_TYPE_B) { contGroup++; }
            else if (type == BLOCK_TYPE_D && (lastType == BLOCK_TYPE_C || lastType == BLOCK_TYPE_CP)) { contGroup++; }
            else { contGroup = 0; }

            // If we've got an entire group, process it
            if (contGroup >= 4) {
                contGroup = 0;
                decodeGroup();
            }

            // // Remember the last block type and skip to new block
            lastType = type;
            skip = BLOCK_LEN;
        }
    }

    uint16_t RDSDecoder::calcSyndrome(uint32_t block) {
        uint16_t syn = 0;

        // Calculate the syndrome using a LFSR
        for (int i = BLOCK_LEN - 1; i >= 0; i--) {
            // Shift the syndrome and keep the output
            uint8_t outBit = (syn >> (POLY_LEN - 1)) & 1;
            syn = (syn << 1) & 0b1111111111;

            // Apply LFSR polynomial
            syn ^= LFSR_POLY * outBit;

            // Apply input polynomial.
            syn ^= IN_POLY * ((block >> i) & 1);
        }

        return syn;
    }

    uint32_t RDSDecoder::correctErrors(uint32_t block, BlockType type, bool& recovered) {        
        // Subtract the offset from block
        block ^= (uint32_t)OFFSETS[type];
        uint32_t out = block;

        // Calculate syndrome of corrected block
        uint16_t syn = calcSyndrome(block);

        // Use the syndrome register to do error correction if errors are present
        uint8_t errorFound = 0;
        if (syn) {
            for (int i = DATA_LEN - 1; i >= 0; i--) {
                // Check if the 5 leftmost bits are all zero
                errorFound |= !(syn & 0b11111);

                // Write output
                uint8_t outBit = (syn >> (POLY_LEN - 1)) & 1;
                out ^= (errorFound & outBit) << (i + POLY_LEN);

                // Shift syndrome
                syn = (syn << 1) & 0b1111111111;
                syn ^= LFSR_POLY * outBit * !errorFound;
            }
        }
        recovered = !(syn & 0b11111);

        return out;
    }

    void RDSDecoder::decodeGroup() {
        std::lock_guard<std::mutex> lck(groupMtx);
        auto now = std::chrono::high_resolution_clock::now();
        anyGroupLastUpdate = now;

        // Make sure blocks A and B are available
        if (!blockAvail[BLOCK_TYPE_A] || !blockAvail[BLOCK_TYPE_B]) { return; }

        // Decode PI code
        countryCode = (blocks[BLOCK_TYPE_A] >> 22) & 0xF;
        programCoverage = (AreaCoverage)((blocks[BLOCK_TYPE_A] >> 18) & 0xF);
        programRefNumber = (blocks[BLOCK_TYPE_A] >> 10) & 0xFF;

        // Decode group type and version
        uint8_t groupType = (blocks[BLOCK_TYPE_B] >> 22) & 0xF;
        GroupVersion groupVer = (GroupVersion)((blocks[BLOCK_TYPE_B] >> 21) & 1);

        // Decode traffic program and program type
        trafficProgram = (blocks[BLOCK_TYPE_B] >> 20) & 1;
        programType = (ProgramType)((blocks[BLOCK_TYPE_B] >> 15) & 0x1F);
        
        if (groupType == 0) {
            group0LastUpdate = now;
            trafficAnnouncement = (blocks[BLOCK_TYPE_B] >> 14) & 1;
            music = (blocks[BLOCK_TYPE_B] >> 13) & 1;
            uint8_t diBit = (blocks[BLOCK_TYPE_B] >> 12) & 1;
            uint8_t offset = ((blocks[BLOCK_TYPE_B] >> 10) & 0b11);
            uint8_t diOffset = 3 - offset;
            uint8_t psOffset = offset * 2;

            if (groupVer == GROUP_VER_A && blockAvail[BLOCK_TYPE_C]) {
                alternateFrequency = (blocks[BLOCK_TYPE_C] >> 10) & 0xFFFF;
            }

            // Write DI bit to the decoder identification
            decoderIdent &= ~(1 << diOffset);
            decoderIdent |= (diBit << diOffset);

            // Write chars at offset the PSName
            if (blockAvail[BLOCK_TYPE_D]) {
                programServiceName[psOffset] = (blocks[BLOCK_TYPE_D] >> 18) & 0xFF;
                programServiceName[psOffset + 1] = (blocks[BLOCK_TYPE_D] >> 10) & 0xFF;
            }
        }
        else if (groupType == 2) {
            group2LastUpdate = now;
            // Get char offset and write chars in the Radiotext
            bool nAB = (blocks[BLOCK_TYPE_B] >> 14) & 1;
            uint8_t offset = (blocks[BLOCK_TYPE_B] >> 10) & 0xF;

            // Clear text field if the A/B flag changed
            if (nAB != rtAB) {
                radioText = "                                                                ";
            }
            rtAB = nAB;

            // Write char at offset in Radiotext
            if (groupVer == GROUP_VER_A) {
                uint8_t rtOffset = offset * 4;
                if (blockAvail[BLOCK_TYPE_C]) {
                    radioText[rtOffset] = (blocks[BLOCK_TYPE_C] >> 18) & 0xFF;
                    radioText[rtOffset + 1] = (blocks[BLOCK_TYPE_C] >> 10) & 0xFF;
                }
                if (blockAvail[BLOCK_TYPE_D]) {
                    radioText[rtOffset + 2] = (blocks[BLOCK_TYPE_D] >> 18) & 0xFF;
                    radioText[rtOffset + 3] = (blocks[BLOCK_TYPE_D] >> 10) & 0xFF;
                }
            }
            else {
                uint8_t rtOffset = offset * 2;
                if (blockAvail[BLOCK_TYPE_D]) {
                    radioText[rtOffset] = (blocks[BLOCK_TYPE_D] >> 18) & 0xFF;
                    radioText[rtOffset + 1] = (blocks[BLOCK_TYPE_D] >> 10) & 0xFF;
                }
            }
        }
    }

    std::string RDSDecoder::convertToUTF8(std::string RT){
        //Iterates through all bytes in the Radiotext buffer and converts them to UTF-8 using a lookup table

        std::string out = ""; //The converted string will be appended to this variable
        for (size_t i = 0; i < RT.length(); i++) //For each byte in the original Radiotext buffer
        {
            uint8_t currentByte = RT[i] & 0xff; //Make sure we have a 8 bit value to compare against
            out.append(EBUtoUTF8Table[currentByte]);
        }

        return out;
        
    }

    bool RDSDecoder::anyGroupValid() {
        auto now = std::chrono::high_resolution_clock::now();
        return (std::chrono::duration_cast<std::chrono::milliseconds>(now - anyGroupLastUpdate)).count() < 5000.0;
    }

    bool RDSDecoder::group0Valid() {
        auto now = std::chrono::high_resolution_clock::now();
        return (std::chrono::duration_cast<std::chrono::milliseconds>(now - group0LastUpdate)).count() < 5000.0;
    }

    bool RDSDecoder::group2Valid() {
        auto now = std::chrono::high_resolution_clock::now();
        return (std::chrono::duration_cast<std::chrono::milliseconds>(now - group2LastUpdate)).count() < 5000.0;
    }
}