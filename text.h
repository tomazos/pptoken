#pragma once

#include <string>
#include <vector>

namespace ppt {

bool IsAlpha(int code_point);
bool IsDigit(int code_point);
bool IsOctDigit(int code_point);
bool IsHexDigit(int code_point);
bool IsSpace(int code_point);
bool IsValidCodePoint(int code_point);

int HexCharToValue(int c);
char ValueToHexChar(int c);
std::string HexDump(void* pdata, size_t nbytes);
inline std::string HexDump(const std::vector<std::byte>& data) {
  return HexDump((void*)data.data(), data.size());
}

std::string EncodeUtf8(const std::vector<int>& input);
std::vector<int> DecodeUtf8(const std::string& input);

template <typename T>
inline std::vector<std::byte> to_byte_vector(const T& t) {
  std::byte* begin = (std::byte*)&t;
  std::byte* end = begin + sizeof(T);

  return {begin, end};
}

struct Utf8Decoder {
  std::vector<int> decode(int input);

 private:
  int x = 0;
  int y = 0;
};

std::u16string EncodeUtf16(const std::vector<int>& input);
std::vector<int> DecodeUtf16(const std::u16string& input);

bool IsAllowedIdentifierFirstCharacter(int c);
bool IsAllowedIdentifierBodyCharacter(int c);

}  // namespace ppt
