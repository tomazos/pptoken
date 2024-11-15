#pragma once

#include <array>

namespace ppt::idx {

// 64-bit only
static_assert(sizeof(size_t) == 8);

// File starts with...
struct IndexHeader {
  std::array<char, 4> magic = {'p', 'p', 't', 'I'};
  uint32_t version = 2;
  size_t code_section_offset;  // start-of-file relative
  size_t code_section_length;  // bytes
  size_t file_section_offset;  // start-of-file relative
  size_t num_files;
  size_t total_tokens;
  size_t total_lines;
  size_t total_bytes;
  size_t token_id_section_offset;            // start-of-file relative
  size_t token_alphabetical_section_offset;  // start-of-file relative
  size_t num_tokens;
};
static_assert(sizeof(IndexHeader) == 88);
static_assert(alignof(IndexHeader) == 8);

// At code_section_offset there is an array of code_section_length bytes
// that is the encoded source code of the dataset.  The encoding is
// a variable-length encoding of the token ids of the sequence of tokens
// that appear in each source file.  Lower ids are assigned to more
// frequently occurring tokens, and the lower the id the shorter the
// number of bytes to encode it.  The encoding also has the property
// that: a sequence of tokens A is a subsequence of a sequence of
// tokens B if and only if the encoded bytes of A are a subsequence
// of the encoded bytes of B.  This means you can search on a bytewise
// basis without logical error.  For details of the code section encoding
// see token_codec.h

// At file_section_offset there is an array of num_files FileInfo...
struct FileInfo {
  size_t filename_cstr;  // offset start-of-file relative (C string)
  size_t file_length;
  size_t code_offset;  // relative to IndexHeader.code_section_offset
  size_t code_length;  // bytes
  size_t num_lines;
  size_t lineinfo_offset;  // start-of-file relative
};
static_assert(sizeof(FileInfo) == 48);
static_assert(alignof(FileInfo) == 8);

// At token_id_section_offset there is an array of num_tokens TokenInfo
// in token id order.  The first is token id 1 (token id 0 means EOF)
struct TokenIdInfo {
  size_t spelling_cstr;  // offset start-of-file relative (C string)
};

// At token_alphabetical_section_offset there is an array of num_tokens
// TokenAlphabeticalInfo
struct TokenAlphabeticalInfo {
  uint32_t token_id;
};

// At each FileInfo.lineinfo_offset there is an array of FileInfo.num_lines
// LineInfo records.
struct LineInfo {
  uint32_t file_offset;  // relative to start of source file
  uint32_t code_offset;  // relative to start of code for file in code section
};

}  // namespace ppt::idx
