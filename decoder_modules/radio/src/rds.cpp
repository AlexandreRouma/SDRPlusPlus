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

    // This parity check matrix is given in annex B2.1 of the specificiation
    const uint16_t PARITY_CHECK_MAT[] = {
        0b1000000000,
        0b0100000000,
        0b0010000000,
        0b0001000000,
        0b0000100000,
        0b0000010000,
        0b0000001000,
        0b0000000100,
        0b0000000010,
        0b0000000001,
        0b1011011100,
        0b0101101110,
        0b0010110111,
        0b1010000111,
        0b1110011111,
        0b1100010011,
        0b1101010101,
        0b1101110110,
        0b0110111011,
        0b1000000001,
        0b1111011100,
        0b0111101110,
        0b0011110111,
        0b1010100111,
        0b1110001111,
        0b1100011011
    };

    const int BLOCK_LEN = 26;
    const int DATA_LEN = 16;
    const int POLY_LEN = 10;
    const uint16_t LFSR_POLY = 0x5B9;
    const uint16_t IN_POLY = 0x31B;

    void RDSDecoder::process(uint8_t* symbols, int count) {
        for (int i = 0; i < count; i++) {
            // Shift in the bit
            shiftReg = ((shiftReg << 1) & 0x3FFFFFF) | (symbols[i] & 1);

            // Skip if we need to shift in new data
            if (--skip > 0) {
                continue;
            }

            // Calculate the syndrome and update sync status
            uint16_t syn = calcSyndrome(shiftReg);
            auto synIt = SYNDROMES.find(syn);
            bool knownSyndrome = synIt != SYNDROMES.end();
            sync = std::clamp<int>(knownSyndrome ? ++sync : --sync, 0, 4);

            // if (knownSyndrome) {
            //     printf("Found known syn: %04X\n", syn);
            // }
            
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
            blocks[type] = shiftReg;//correctErrors(shiftReg, type);

            //printf("Block type: %d, Sync: %d, KnownSyndrome: %s, contGroup: %d, offset: %d\n", type, sync, knownSyndrome ? "yes" : "no", contGroup, i);

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
        // Perform vector/matrix dot product between block and parity matrix
        uint16_t syn = 0;
        for(int i = 0; i < BLOCK_LEN; i++) {
            syn ^= PARITY_CHECK_MAT[BLOCK_LEN - 1 - i] * ((block >> i) & 1);
        }
        return syn;
    }

    uint32_t RDSDecoder::correctErrors(uint32_t block, BlockType type) {
        // Init the syndrome and output
        uint16_t syn = 0;
        uint32_t out = block;

        // Subtract the offset from block
        block ^= (uint32_t)OFFSETS[type];

        // Feed in the data
        for (int i = BLOCK_LEN - 1; i >= 0; i--) {
            // Shift the syndrome and keep the output
            uint8_t outBit = (syn >> (POLY_LEN - 1)) & 1;
            syn = (syn << 1) & 0b1111111111;

            // Apply LFSR polynomial
            syn ^= LFSR_POLY * outBit;

            // Apply input polynomial.
            syn ^= IN_POLY * ((block >> i) & 1);
        }

        // Use the syndrome register to do error correction
        // TODO: For some reason there's always zeros in the syn when starting
        uint8_t errorFound = 0;
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

        // TODO: mark block as irrecoverable if too damaged

        if (errorFound) {
            printf("Error found\n");
        }

        return out;
    }

    void RDSDecoder::decodeGroup() {
        std::lock_guard<std::mutex> lck(groupMtx);
        auto now = std::chrono::high_resolution_clock::now();
        anyGroupLastUpdate = now;

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

            if (groupVer == GROUP_VER_A) {
                alternateFrequency = (blocks[BLOCK_TYPE_C] >> 10) & 0xFFFF;
            }

            // Write DI bit to the decoder identification
            decoderIdent &= ~(1 << diOffset);
            decoderIdent |= (diBit << diOffset);

            // Write chars at offset the PSName
            programServiceName[psOffset] = (blocks[BLOCK_TYPE_D] >> 18) & 0xFF;
            programServiceName[psOffset + 1] = (blocks[BLOCK_TYPE_D] >> 10) & 0xFF;
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
                radioText[rtOffset] = (blocks[BLOCK_TYPE_C] >> 18) & 0xFF;
                radioText[rtOffset + 1] = (blocks[BLOCK_TYPE_C] >> 10) & 0xFF;
                radioText[rtOffset + 2] = (blocks[BLOCK_TYPE_D] >> 18) & 0xFF;
                radioText[rtOffset + 3] = (blocks[BLOCK_TYPE_D] >> 10) & 0xFF;
            }
            else {
                uint8_t rtOffset = offset * 2;
                radioText[rtOffset] = (blocks[BLOCK_TYPE_D] >> 18) & 0xFF;
                radioText[rtOffset + 1] = (blocks[BLOCK_TYPE_D] >> 10) & 0xFF;
            }
        }
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