#ifndef BIT_EXTRACTOR_H
#define BIT_EXTRACTOR_H

#include <cstdint>

class BitExtractor
{
public:
    // Extract bits from CAN frame payload
    // payload: CAN frame data (up to 8 bytes for classic CAN)
    // byteOffset: starting byte (0-7)
    // bitStart: starting bit within the extracted bytes (0 = LSB)
    // bitLength: number of bits to extract (1-32)
    // littleEndian: byte order for multi-byte fields
    // signExtend: sign-extend the result for signed values
    static int32_t extract(const uint8_t* payload,
                           int byteOffset,
                           int bitStart,
                           int bitLength,
                           bool littleEndian,
                           bool signExtend);

    // Pack value into CAN frame payload (for commands)
    // payload: destination buffer
    // byteOffset: starting byte
    // bitLength: number of bits (determines how many bytes to write)
    // littleEndian: byte order
    // value: value to pack
    static void pack(uint8_t* payload,
                     int byteOffset,
                     int bitLength,
                     bool littleEndian,
                     int32_t value);
};

#endif // BIT_EXTRACTOR_H
