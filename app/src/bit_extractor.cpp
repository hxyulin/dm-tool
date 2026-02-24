#include "bit_extractor.h"

int32_t BitExtractor::extract(const uint8_t* payload,
                              int byteOffset,
                              int bitStart,
                              int bitLength,
                              bool littleEndian,
                              bool signExtend)
{
    if (bitLength <= 0 || bitLength > 32 || byteOffset < 0) {
        return 0;
    }

    // Calculate how many bytes we need to read
    int totalBits = bitStart + bitLength;
    int bytesNeeded = (totalBits + 7) / 8;

    // Build raw value from bytes
    uint32_t raw = 0;
    if (littleEndian) {
        // Little endian: LSB first
        for (int i = 0; i < bytesNeeded && (byteOffset + i) < 8; ++i) {
            raw |= static_cast<uint32_t>(payload[byteOffset + i]) << (i * 8);
        }
    } else {
        // Big endian: MSB first
        for (int i = 0; i < bytesNeeded && (byteOffset + i) < 8; ++i) {
            raw = (raw << 8) | payload[byteOffset + i];
        }
        // For big endian, we need to align the extracted bits properly
        // The bits we want are at the "top" of the value after reading
        int shift = (bytesNeeded * 8) - totalBits;
        if (shift > 0) {
            raw >>= shift;
        }
    }

    // Extract the specific bits
    uint32_t mask = (bitLength == 32) ? 0xFFFFFFFF : ((1U << bitLength) - 1);
    raw = (raw >> bitStart) & mask;

    // Sign extend if needed
    if (signExtend && bitLength < 32) {
        uint32_t signBit = 1U << (bitLength - 1);
        if (raw & signBit) {
            raw |= ~mask; // Sign extend by setting all upper bits
        }
    }

    return static_cast<int32_t>(raw);
}

void BitExtractor::pack(uint8_t* payload,
                        int byteOffset,
                        int bitLength,
                        bool littleEndian,
                        int32_t value)
{
    if (bitLength <= 0 || bitLength > 32 || byteOffset < 0) {
        return;
    }

    int bytesNeeded = (bitLength + 7) / 8;
    uint32_t raw = static_cast<uint32_t>(value);

    // Mask to the bit length
    uint32_t mask = (bitLength == 32) ? 0xFFFFFFFF : ((1U << bitLength) - 1);
    raw &= mask;

    if (littleEndian) {
        // Little endian: LSB first
        for (int i = 0; i < bytesNeeded && (byteOffset + i) < 8; ++i) {
            payload[byteOffset + i] = static_cast<uint8_t>(raw & 0xFF);
            raw >>= 8;
        }
    } else {
        // Big endian: MSB first
        for (int i = bytesNeeded - 1; i >= 0 && (byteOffset + i) < 8; --i) {
            payload[byteOffset + (bytesNeeded - 1 - i)] = static_cast<uint8_t>((raw >> (i * 8)) & 0xFF);
        }
    }
}
