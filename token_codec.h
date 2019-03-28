#pragma once

#include <cstdint>

#include "dvc/log.h"

namespace ppt {

template <int byte_count>
inline uint8_t encode_leading(uint8_t fourbits) {
  return 0b10000000 | ((byte_count - 1) << 4) | fourbits;
}
inline uint8_t encode_trailing(uint8_t sixbits) { return 0b11000000 | sixbits; }

inline void write_byte(std::byte*& output, uint8_t x) {
  (*output) = std::byte(x);
  output++;
}

inline uint8_t read_byte(const std::byte*& input) {
  uint8_t x = uint8_t(*input);
  input++;
  return x;
}

template <int shift>
inline uint32_t decode_trailing(const std::byte*& input) {
  return uint32_t(read_byte(input) & 0b00111111) << shift;
}

inline void encode_token(uint32_t input, std::byte*& output) {
  if (input < (1u << 7)) {
    write_byte(output, input);
  } else if (input < (1u << 10)) {
    write_byte(output, encode_leading<1>(input & 0b1111));
    input >>= 4;
    write_byte(output, encode_trailing(input));
  } else if (input < (1u << 16)) {
    write_byte(output, encode_leading<2>(input & 0b1111));
    input >>= 4;
    write_byte(output, encode_trailing(input & 0b111111));
    input >>= 6;
    write_byte(output, encode_trailing(input));

  } else if (input < (1u << 22)) {
    write_byte(output, encode_leading<3>(input & 0b1111));
    input >>= 4;
    write_byte(output, encode_trailing(input & 0b111111));
    input >>= 6;
    write_byte(output, encode_trailing(input & 0b111111));
    input >>= 6;
    write_byte(output, encode_trailing(input));

  } else if (input < (1u << 28)) {
    write_byte(output, encode_leading<4>(input & 0b1111));
    input >>= 4;
    write_byte(output, encode_trailing(input & 0b111111));
    input >>= 6;
    write_byte(output, encode_trailing(input & 0b111111));
    input >>= 6;
    write_byte(output, encode_trailing(input & 0b111111));
    input >>= 6;
    write_byte(output, encode_trailing(input));
  } else {
    DVC_FATAL("ppt::encode_token(", input, ")");
  }
}

inline uint32_t decode_token(const std::byte*& input) {
  uint32_t first = read_byte(input);
  if (!(first & 0b10000000)) {
    return first;
  }
  if ((first & 0b00110000) == 0b00000000) {
    return (first & 0b1111) | decode_trailing<4>(input);
  } else if ((first & 0b00110000) == 0b00010000) {
    return (first & 0b1111) | decode_trailing<4>(input) |
           decode_trailing<10>(input);
  } else if ((first & 0b00110000) == 0b00100000) {
    return (first & 0b1111) | decode_trailing<4>(input) |
           decode_trailing<10>(input) | decode_trailing<16>(input);
  } else /*((first & 0b00110000) == 0b00110000)*/ {
    return (first & 0b1111) | decode_trailing<4>(input) |
           decode_trailing<10>(input) | decode_trailing<16>(input) |
           decode_trailing<22>(input);
  }
}

}  // namespace ppt
