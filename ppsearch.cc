#include <atomic>
#include <map>
#include <random>
#include <thread>

#include "dvc/file.h"
#include "dvc/opts.h"
#include "dvc/program.h"
#include "dvc/sampler.h"
#include "index_reader.h"
#include "mmapfile.h"
#include "token_codec.h"
#include "tokenize.h"
#include "vector_token_stream.h"

namespace ppt {

std::filesystem::path DVC_OPTION(index_file, -, dvc::required,
                                 "input index file");

std::string DVC_OPTION(query, q, dvc::required, "query");

size_t DVC_OPTION(nthreads, n, dvc::required, "number of threads");

size_t DVC_OPTION(block_size, b, dvc::required, "number of blocks");

constexpr size_t num_matches = 100;

void ppsearch(int argc, char** argv) {
  dvc::program program(argc, argv);

  DVC_ASSERT(exists(index_file), "No such file: ", index_file);
  mmapfile index_mmap(index_file);
  idx::IndexReader index(index_mmap.get());

  if (query.empty()) DVC_FAIL("Empty query string.");

  VectorTokenStream output;
  try {
    Tokenize(query, output);
  } catch (std::exception& e) {
    DVC_FAIL("Could not tokenize query string `", query,
             "` because: ", e.what());
  }
  if (output.tokens.empty()) DVC_FAIL("Query string contains no C++ tokens.");

  std::vector<std::byte> encoded(5 * (output.tokens.size() + 1));
  std::byte* ptr = encoded.data();
  for (const Token& token : output.tokens) {
    uint32_t token_id = index.token_id(token.spelling);
    if (token_id == 0)
      DVC_FAIL("No matches found.  (No such token in dataset `", token.spelling,
               "`)");
    encode_token(token_id, ptr);
  }
  encoded.resize(ptr - encoded.data());
  DVC_ASSERT_GT(encoded.size(), 0);

  static const std::byte* query_begin = encoded.data();
  static const std::byte* query_end = encoded.data() + encoded.size();
  const std::byte* code_section_end = index.code + index.code_length;

  dvc::sampler<const std::byte*, num_matches> matches;

  std::vector<std::thread> threads;
  std::atomic_size_t next_block = 0;
  std::atomic_size_t bytes_searched = 0;
  for (size_t thread_index = 0; thread_index < nthreads; thread_index++)
    threads.emplace_back([&, thread_index] {
      while (true) {
        size_t block = next_block++;
        const std::byte* start = index.code + block * block_size;
        if (start >= code_section_end) return;
        const std::byte* end = start + block_size;
        if (end > code_section_end) end = code_section_end;

        for (const std::byte* candidate = start; candidate < end; candidate++) {
          bool found = true;
          const std::byte* p = candidate;
          for (const std::byte* q = query_begin; q < query_end; q++) {
            if (*p != *q) {
              found = false;
              break;
            }
            p++;
          }
          if (found) {
            matches(candidate);
          }
        }
      }
    });
  for (std::thread& t : threads) t.join();
  threads.clear();

  DVC_DUMP(matches.size());
  std::vector<const std::byte*> samples = matches.build_samples();
  for (const std::byte* sample : samples) {
    idx::IndexReader::FileLines file_lines =
        index.symbolize(sample, encoded.size(), 2);
    std::filesystem::path filename = index.filename(file_lines.file_info);
    DVC_DUMP(filename);
    dvc::file_reader file(filename);
    file.seek(file_lines.first_line.file_offset);

    std::string line = file.read_string(file_lines.last_line.file_offset -
                                        file_lines.first_line.file_offset);
    DVC_DUMP(line);
  }
}

}  // namespace ppt

int main(int argc, char** argv) { ppt::ppsearch(argc, argv); }
