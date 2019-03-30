#pragma once

#include <algorithm>
#include <string_view>

#include "index.h"

#include "dvc/log.h"

namespace ppt::idx {

class File;

class IndexReader {
 public:
  IndexReader(std::string_view index)
      : index_(index), header_(to_ptr<IndexHeader>(0)) {
    DVC_ASSERT(IndexHeader{}.magic == header_->magic);
    DVC_ASSERT_EQ(IndexHeader{}.version, header_->version);
    file_infos = to_ptr<FileInfo>(header_->file_section_offset);
    num_files = header_->num_files;

    token_ids = to_ptr<TokenIdInfo>(header_->token_id_section_offset);
    token_alphas = to_ptr<TokenAlphabeticalInfo>(
        header_->token_alphabetical_section_offset);
    num_tokens = header_->num_tokens;

    code = to_ptr<std::byte>(header_->code_section_offset);
    code_length = header_->code_section_length;

    total_bytes = header_->total_bytes;
    total_tokens = header_->total_tokens;
    total_lines = header_->total_lines;
  }

  const FileInfo* file_infos;
  size_t num_files;

  size_t total_bytes;
  size_t total_tokens;
  size_t total_lines;

  std::filesystem::path filename(const FileInfo& file_info) {
    return cstr(file_info.filename_cstr);
  }

  const std::byte* filecode(const FileInfo& file_info) {
    return code + file_info.code_offset;
  }

  const LineInfo* line_infos(const FileInfo& file_info) {
    return to_ptr<LineInfo>(file_info.lineinfo_offset);
  }

  const TokenIdInfo* token_ids;
  const TokenAlphabeticalInfo* token_alphas;
  size_t num_tokens;

  std::string_view spelling(uint32_t token_id) {
    if (token_id == 0) return "";
    size_t i = token_id - 1;
    DVC_ASSERT_LT(i, num_tokens);
    return cstr(token_ids[i].spelling_cstr);
  }

  uint32_t token_id(std::string_view token_spelling) {
    const TokenAlphabeticalInfo* candidate =
        std::partition_point(token_alphas, token_alphas + num_tokens,
                             [&](const TokenAlphabeticalInfo& info) {
                               return spelling(info.token_id) < token_spelling;
                             });
    if (candidate == token_alphas + num_tokens)
      return 0;
    else if (spelling(candidate->token_id) == token_spelling)
      return candidate->token_id;
    else
      return 0;
  }

  const std::byte* code;
  size_t code_length;

  struct FileLines {
    const idx::FileInfo& file_info;
    uint32_t first_lineno;
    uint32_t match_lineno;
    const idx::LineInfo* lines;
    uint32_t num_lines;
  };

  FileLines symbolize(const std::byte* pos, uint32_t len, uint32_t context) {
    size_t pos_offset = pos - code;
    DVC_ASSERT_LT(pos_offset, code_length, "symbolize out of bounds");
    const FileInfo* file_info = std::partition_point(
        file_infos, file_infos + num_files, [&](const FileInfo& info) {
          return info.code_offset + info.code_length <= pos_offset;
        });

    DVC_ASSERT_GE(file_info, file_infos);
    DVC_ASSERT_LT(file_info, file_infos + num_files);
    DVC_ASSERT_GE(pos_offset, file_info->code_offset);
    uint32_t begin_code = pos_offset - file_info->code_offset;
    DVC_ASSERT_LT(begin_code, file_info->code_length);
    uint32_t end_code = begin_code + len;
    DVC_ASSERT_LT(end_code, file_info->code_length);
    const idx::LineInfo* file_line_infos = line_infos(*file_info);

    auto find_line = [&](uint32_t code_offset, bool first) -> const LineInfo* {
      const LineInfo* line_info =
          std::partition_point(file_line_infos + 1,
                               file_line_infos + file_info->num_lines,
                               [&](const LineInfo& info) {
                                 return info.code_offset <= code_offset;
                               }) -
          1;
      DVC_ASSERT_GE(line_info, file_line_infos);
      DVC_ASSERT_LT(line_info, file_line_infos + file_info->num_lines);
      if (!first) {
        while (line_info > file_line_infos &&
               (line_info - 1)->code_offset == code_offset)
          line_info--;
      }
      DVC_ASSERT_GE(line_info, file_line_infos);
      DVC_ASSERT_LT(line_info, file_line_infos + file_info->num_lines);
      return line_info;
    };

    const LineInfo* first_line = find_line(begin_code, true);
    const LineInfo* last_line = find_line(end_code, false);
    if (first_line == last_line) last_line++;
    DVC_ASSERT_LT(last_line, file_line_infos + file_info->num_lines);

    uint32_t match_lineno = 1 + (first_line - file_line_infos);

    for (size_t i = 0; i < context; i++) {
      if (first_line > file_line_infos) first_line--;
      if (last_line < file_line_infos + file_info->num_lines - 1) last_line++;
    }
    DVC_ASSERT_GE(first_line, file_line_infos);
    DVC_ASSERT_LT(first_line, file_line_infos + file_info->num_lines);
    DVC_ASSERT_GE(last_line, file_line_infos);
    DVC_ASSERT_LT(last_line, file_line_infos + file_info->num_lines);

    uint32_t first_lineno = 1 + (first_line - file_line_infos);
    const idx::LineInfo* lines = first_line;
    uint32_t num_lines = last_line - first_line;

    return {*file_info, first_lineno, match_lineno, lines, num_lines};
  }

 private:
  std::string_view cstr(size_t offset) { return to_ptr<char>(offset); }
  template <typename T>
  const T* to_ptr(size_t offset) {
    return (const T*)(index_.data() + offset);
  }
  std::string_view index_;
  const IndexHeader* header_;
};

}  // namespace ppt::idx
