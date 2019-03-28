#include "dvc/file.h"
#include "dvc/opts.h"
#include "dvc/program.h"
#include "index_reader.h"
#include "mmapfile.h"

namespace ppt {

std::filesystem::path DVC_OPTION(index_file, -, dvc::required,
                                 "input index file");

void ppverify(int argc, char** argv) {
  dvc::program program(argc, argv);

  DVC_ASSERT(exists(index_file), "No such file: ", index_file);

  mmapfile index_mmap(index_file);

  idx::IndexReader index(index_mmap.get());

  DVC_DUMP(index.num_files);

  const std::byte* code = index.code;

  for (size_t i = 0; i < index.num_files; i++) {
    const idx::FileInfo& file_info = index.file_infos[i];
    std::filesystem::path path = index.filename(file_info);
    DVC_ASSERT(exists(path));
    size_t file_length = file_size(path);

    DVC_ASSERT_EQ(code, index.filecode(file_info));
    code += file_info.code_length;

    const idx::LineInfo* line_infos = index.line_infos(file_info);
    for (size_t i = 0; i < file_info.num_lines; i++) {
      const idx::LineInfo& line_info = line_infos[i];
      DVC_ASSERT_LE(line_info.file_offset, file_length);
      DVC_ASSERT_LT(line_info.code_offset, file_info.code_length);

      if (i != 0) {
        DVC_ASSERT_LT(line_infos[i - 1].file_offset, line_infos[i].file_offset);
        DVC_ASSERT_LE(line_infos[i - 1].code_offset, line_infos[i].code_offset);
      }
    }
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
