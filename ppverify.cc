#include "dvc/file.h"
#include "dvc/opts.h"
#include "dvc/program.h"
#include "index_reader.h"
#include "mmapfile.h"

namespace ppt {

std::filesystem::path DVC_OPTION(index_file, -, dvc::required,
                                 "input index file");

bool DVC_OPTION(verbose, v, false, "verbose");

void ppverify(int argc, char** argv) {
  dvc::program program(argc, argv);

  DVC_ASSERT(exists(index_file), "No such file: ", index_file);

  mmapfile index_mmap(index_file);

  idx::IndexReader index(index_mmap.get());

  DVC_DUMP(index.num_files);
  DVC_DUMP(index.code_length);
  DVC_DUMP(index.num_tokens);
  DVC_DUMP(index.total_bytes);
  DVC_DUMP(index.total_lines);
  DVC_DUMP(index.total_tokens);

  const std::byte* code = index.code;

  for (size_t i = 0; i < index.num_files; i++) {
    const idx::FileInfo& file_info = index.file_infos[i];
    std::filesystem::path path = index.filename(file_info);
    DVC_ASSERT(exists(path));
    size_t file_length = file_size(path);
    if (verbose) {
      DVC_LOG("FILE #", i, ": ", path);
      DVC_DUMP(file_info.code_length);
      DVC_DUMP(file_info.code_offset);
      DVC_DUMP(file_info.file_length);
      DVC_DUMP(file_info.lineinfo_offset);
      DVC_DUMP(file_info.num_lines);
    }

    DVC_ASSERT_EQ(file_length, file_info.file_length);

    DVC_ASSERT_EQ(code, index.filecode(file_info));
    code += file_info.code_length;

    DVC_ASSERT_LE(2, file_info.num_lines);
    const idx::LineInfo* line_infos = index.line_infos(file_info);
    for (size_t j = 0; j < file_info.num_lines; j++) {
      const idx::LineInfo& line_info = line_infos[j];

      if (verbose) {
        DVC_LOG("LINE #", j, " code_offset=", line_info.code_offset,
                " file_offet=", line_info.file_offset);
      }
      DVC_ASSERT_LE(line_info.file_offset, file_length);
      DVC_ASSERT_LT(line_info.code_offset, file_info.code_length);

      if (j != 0) {
        DVC_ASSERT_LE(line_infos[j - 1].file_offset, line_infos[j].file_offset);
        DVC_ASSERT_LE(line_infos[j - 1].code_offset, line_infos[j].code_offset);
      }
    }
    DVC_ASSERT_EQ(line_infos[0].file_offset, 0);
    DVC_ASSERT_EQ(line_infos[0].code_offset, 0);
    DVC_ASSERT_EQ(line_infos[file_info.num_lines - 1].file_offset, file_length);
    DVC_ASSERT_EQ(line_infos[file_info.num_lines - 1].code_offset,
                  file_info.code_length - 1);
  }

  DVC_ASSERT_EQ(code, index.code + index.code_length);

  DVC_DUMP(index.num_tokens);
  for (size_t i = 0; i <= index.num_tokens; i++) {
    std::string spelling = std::string(index.spelling(i));
    DVC_ASSERT_EQ(i, index.token_id(spelling));
  }
  DVC_ASSERT_EQ(0, index.token_id("fofasjfgksabgkjasgfnbsakj"));
}

}  // namespace ppt

int main(int argc, char** argv) { ppt::ppverify(argc, argv); }
