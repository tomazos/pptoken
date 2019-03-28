#include "token_codec.h"

#include "dvc/log.h"
#include "dvc/program.h"

namespace {

std::byte b[10];

void test_round_trip(uint32_t code_in) {
  std::byte* pos1 = b;
  const std::byte* pos2 = b;

  ppt::encode_token(code_in, pos1);
  size_t len = pos1 - b;
  DVC_ASSERT_LE(len, 5);
  if (len == 1)
    DVC_ASSERT(!(uint8_t(b[0]) & 0b10000000));
  else {
    DVC_ASSERT_EQ(uint8_t(b[0]) & 0b11000000, 0b10000000);
    for (size_t i = 1; i < len; i++)
      DVC_ASSERT_EQ(uint8_t(b[i]) & 0b11000000, 0b11000000);
    DVC_ASSERT_EQ((uint8_t(b[0]) & 0b00110000) >> 4, len - 2, code_in);
  }

  uint32_t code_out = ppt::decode_token(pos2);
  DVC_ASSERT_EQ(len, size_t(pos2 - b));
  DVC_ASSERT_EQ(code_in, code_out);
}

}  // namespace

int main() {
  dvc::program program;

  for (uint32_t i = 0; i < (1u << 27); i++) test_round_trip(i);
}
