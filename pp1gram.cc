#include <unordered_map>
#include <unordered_set>
#include <map>

#include "dvc/file.h"
#include "dvc/opts.h"
#include "dvc/program.h"
#include "index_reader.h"
#include "mmapfile.h"
#include "token_codec.h"

namespace ppt {

std::filesystem::path DVC_OPTION(index_file, -, dvc::required,
                                 "input index file");

void ppverify(int argc, char** argv) {
  dvc::program program(argc, argv);

  DVC_ASSERT(exists(index_file), "No such file: ", index_file);

  mmapfile index_mmap(index_file);

  idx::IndexReader index(index_mmap.get());

  std::unordered_map<uint32_t, uint32_t> token_counts;

  for (size_t i = 0; i < index.num_files; i++) {
	  if ((i & (i-1)) == 0) DVC_DUMP(i);
    const idx::FileInfo& file_info = index.file_infos[i];

    const std::byte* code = index.filecode(file_info);

    std::unordered_set<uint32_t> tokens;

    while (true) {
      uint32_t token = decode_token(code);
      if (token == 0)
	      break;

      tokens.insert(token);
    }

    for (uint32_t token : tokens)
      token_counts[token]++;

    DVC_ASSERT_EQ(code - index.filecode(file_info), ssize_t(file_info.code_length));
  }

  std::multimap<double, uint32_t, std::greater<double>> counts_tokens;

  for (const auto& [token, count] : token_counts)
    counts_tokens.insert(std::pair(100.0 * double(count) / double(index.num_files), token));

  for (const auto& [count, token] : counts_tokens) {
    if (count < 0.01)
      continue;
    std::string spelling{index.spelling(token)};
    std::cout << count << " " << spelling.size() << " ";
    std::cout.write(spelling.data(), spelling.size());
    std::cout << "\n";
  }
}

}  // namespace ppt

int main(int argc, char** argv) { ppt::ppverify(argc, argv); }
