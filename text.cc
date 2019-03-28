#include "text.h"
#include <algorithm>
#include <stdexcept>

namespace ppt {

bool IsSpace(int code_point) {
  if (code_point < 0 || code_point > 127) return false;

  return isspace(code_point);
}

bool IsDigit(int code_point) {
  if (code_point < 0 || code_point > 127) return false;

  return isdigit(code_point);
}

bool IsOctDigit(int code_point) {
  if (code_point < 0 || code_point > 127) return false;

  switch (code_point) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
      return true;
    default:
      return false;
  }
}

bool IsHexDigit(int code_point) {
  if (code_point < 0 || code_point > 127) return false;

  return isxdigit(code_point);
}

bool IsAlpha(int code_point) {
  if (code_point < 0 || code_point > 127) return false;

  return isalpha(code_point);
}

bool IsValidCodePoint(int code_point) {
  return (code_point >= 0 && code_point < 0xD800) ||
         (code_point >= 0xE000 && code_point < 0x110000);
}

int HexCharToValue(int c) {
  switch (c) {
    case '0':
      return 0;
    case '1':
      return 1;
    case '2':
      return 2;
    case '3':
      return 3;
    case '4':
      return 4;
    case '5':
      return 5;
    case '6':
      return 6;
    case '7':
      return 7;
    case '8':
      return 8;
    case '9':
      return 9;
    case 'A':
      return 10;
    case 'a':
      return 10;
    case 'B':
      return 11;
    case 'b':
      return 11;
    case 'C':
      return 12;
    case 'c':
      return 12;
    case 'D':
      return 13;
    case 'd':
      return 13;
    case 'E':
      return 14;
    case 'e':
      return 14;
    case 'F':
      return 15;
    case 'f':
      return 15;
    default:
      throw std::logic_error("HexCharToValue of nonhex char");
  }
}

char ValueToHexChar(int c) {
  switch (c) {
    case 0:
      return '0';
    case 1:
      return '1';
    case 2:
      return '2';
    case 3:
      return '3';
    case 4:
      return '4';
    case 5:
      return '5';
    case 6:
      return '6';
    case 7:
      return '7';
    case 8:
      return '8';
    case 9:
      return '9';
    case 10:
      return 'A';
    case 11:
      return 'B';
    case 12:
      return 'C';
    case 13:
      return 'D';
    case 14:
      return 'E';
    case 15:
      return 'F';
    default:
      throw std::logic_error("ValueToHexChar of nonhex value");
  }
}

std::string HexDump(void* pdata, size_t nbytes) {
  std::byte* p = (std::byte*)pdata;

  std::string s(nbytes * 2, '?');

  for (size_t i = 0; i < nbytes; i++) {
    s[2 * i + 0] = ValueToHexChar((int(p[i]) & 0xF0) >> 4);
    s[2 * i + 1] = ValueToHexChar((int(p[i]) & 0x0F) >> 0);
  }

  return s;
}

std::string EncodeUtf8(const std::vector<int>& input) {
  std::string output;

  for (int c : input) {
    if (c < 0 || c > 0x10FFFF) throw std::logic_error("invalid code point");

    if (c <= 0x7F)
      output += char(c);
    else if (c <= 0x7FF) {
      int b0 = (0xC0 | (c >> 6));
      int b1 = (0x80 | (c & ((1 << 6) - 1)));

      output += char(b0);
      output += char(b1);
    } else if (c <= 0xFFFF) {
      int b0 = (0xE0 | (c >> 12));
      int b1 = (0x80 | ((c >> 6) & ((1 << 6) - 1)));
      int b2 = (0x80 | (c & ((1 << 6) - 1)));

      output += char(b0);
      output += char(b1);
      output += char(b2);
    } else {
      int b0 = (0xF0 | (c >> 18));
      int b1 = (0x80 | ((c >> 12) & ((1 << 6) - 1)));
      int b2 = (0x80 | ((c >> 6) & ((1 << 6) - 1)));
      int b3 = (0x80 | (c & ((1 << 6) - 1)));

      output += char(b0);
      output += char(b1);
      output += char(b2);
      output += char(b3);
    }
  }

  return output;
}

std::vector<int> Utf8Decoder::decode(int input) {
  if (y == 0) {
    if (input == -1)
      return {-1};

    else if (input & 1 << 7) {
      if (!(input & 1 << 6))
        throw std::logic_error("utf8 trailing code unit (10xxxxxx) at start");

      else if (!(input & 1 << 5)) {
        x = input & ((1 << 5) - 1);
        y = 1;
      } else if (!(input & 1 << 4)) {
        x = input & ((1 << 4) - 1);
        y = 2;
      } else if (!(input & 1 << 3)) {
        x = input & ((1 << 3) - 1);
        y = 3;
      } else
        throw std::logic_error("utf8 invalid unit (111111xx)");

      return {};
    } else
      return {input};
  } else {
    if ((input == -1) || !(input & 1 << 7) || (input & 1 << 6))
      throw std::logic_error("utf8 expected trailing byte (10xxxxxx)");

    x <<= 6;
    x |= (input & ((1 << 6) - 1));
    y--;
    if (y == 0)
      return {x};
    else
      return {};
  }
}

std::vector<int> DecodeUtf8(const std::string& input) {
  std::vector<int> output;

  Utf8Decoder decoder;

  for (char c : input) {
    unsigned char code_unit = c;

    for (int code_point : decoder.decode(code_unit))
      output.push_back(code_point);
  }

  return output;
}

std::u16string EncodeUtf16(const std::vector<int>& input) {
  std::u16string s;

  for (int x : input) {
    if (x < 0 || (x >= 0xD800 && x < 0xE000) || x >= 0x110000)
      throw std::logic_error("utf16 encoding error: " + x);

    if (x < 0x10000)
      s += char16_t(x);
    else {
      int U = x - 0x10000;
      int W1 = 0xD800;
      int W2 = 0xDC00;

      W1 |= (U >> 10);
      W2 |= (U & ((1 << 10) - 1));

      s += char16_t(W1);
      s += char16_t(W2);
    }
  }

  return s;
}

std::vector<int> DecodeUtf16(const std::u16string& input) {
  std::vector<int> output;

  int state = 0;

  int W1 = 0, W2;

  for (char16_t c : input) {
    int x = (unsigned short int)c;

    switch (state) {
      case 0:
        if (x >= 0 && (x < 0xD800 || x >= 0xE000) && x < 0x110000)
          output.push_back(x);
        else if (x >= 0xD800 && x < 0xDC00) {
          W1 = x;
          state = 1;
        } else
          throw std::logic_error("invalid UTF-16 code unit: " +
                                 std::to_string(x));

        break;

      case 1:
        if (x >= 0xDC00 && x < 0xE000) {
          W2 = x;

          int U = (W1 & ((1 << 10) - 1)) << 10 | (W2 & ((1 << 10) - 1));
          output.push_back(U + 0x10000);

          state = 0;
        } else
          throw std::logic_error("invalud UTF-16 code units: " +
                                 std::to_string(W1) + " " + std::to_string(x));

        break;

      default:
        throw std::logic_error("DecodeUtf16 unexpected state");
    }
  }

  if (state != 0) throw std::logic_error("unterminated UTF-16 sequence");

  return output;
}

// See C++ Standard Annex E: Universal character names for indentifier
// characters
bool IsCharacterAllowed_E1(int c) {
  static const std::vector<std::pair<int, int>> ranges = {
      {0xA8, 0xA8},       {0xAA, 0xAA},       {0xAD, 0xAD},
      {0xAF, 0xAF},       {0xB2, 0xB5},       {0xB7, 0xBA},
      {0xBC, 0xBE},       {0xC0, 0xD6},       {0xD8, 0xF6},
      {0xF8, 0xFF},       {0x100, 0x167F},    {0x1681, 0x180D},
      {0x180F, 0x1FFF},   {0x200B, 0x200D},   {0x202A, 0x202E},
      {0x203F, 0x2040},   {0x2054, 0x2054},   {0x2060, 0x206F},
      {0x2070, 0x218F},   {0x2460, 0x24FF},   {0x2776, 0x2793},
      {0x2C00, 0x2DFF},   {0x2E80, 0x2FFF},   {0x3004, 0x3007},
      {0x3021, 0x302F},   {0x3031, 0x303F},   {0x3040, 0xD7FF},
      {0xF900, 0xFD3D},   {0xFD40, 0xFDCF},   {0xFDF0, 0xFE44},
      {0xFE47, 0xFFFD},   {0x10000, 0x1FFFD}, {0x20000, 0x2FFFD},
      {0x30000, 0x3FFFD}, {0x40000, 0x4FFFD}, {0x50000, 0x5FFFD},
      {0x60000, 0x6FFFD}, {0x70000, 0x7FFFD}, {0x80000, 0x8FFFD},
      {0x90000, 0x9FFFD}, {0xA0000, 0xAFFFD}, {0xB0000, 0xBFFFD},
      {0xC0000, 0xCFFFD}, {0xD0000, 0xDFFFD}, {0xE0000, 0xEFFFD}};

  auto it = std::upper_bound(
      ranges.begin(), ranges.end(), c,
      [](int a, const std::pair<int, int>& b) { return a < b.first; });
  if (it == ranges.begin())
    return false;
  else
    return (it - 1)->second >= c;
}

bool IsCharacterDisallowedInitially_E2(int c) {
  static const std::vector<std::pair<int, int>> ranges = {
      {0x300, 0x36F}, {0x1DC0, 0x1DFF}, {0x20D0, 0x20FF}, {0xFE20, 0xFE2F}};

  auto it = std::upper_bound(
      ranges.begin(), ranges.end(), c,
      [](int a, const std::pair<int, int>& b) { return a < b.first; });
  if (it == ranges.begin())
    return false;
  else
    return (it - 1)->second >= c;
}

bool IsAllowedIdentifierFirstCharacter(int c) {
  if (c <= 0)
    return false;
  else if (c <= 127)
    return IsAlpha(c) || c == '_';
  else
    return (IsCharacterAllowed_E1(c) && !IsCharacterDisallowedInitially_E2(c));
}

bool IsAllowedIdentifierBodyCharacter(int c) {
  if (c <= 0)
    return false;
  else if (c <= 127)
    return IsAlpha(c) || IsDigit(c) || c == '_';
  else
    return IsCharacterAllowed_E1(c);
}

}  // namespace ppt
