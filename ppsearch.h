#pragma once

#include <atomic>
#include <map>
#include <random>
#include <thread>

#include "dvc/file.h"
#include "dvc/sampler.h"
#include "index_reader.h"
#include "mmapfile.h"
#include "token_codec.h"
#include "tokenize.h"
#include "vector_token_stream.h"

namespace ppt {

struct CodeSearchResults {
  std::string error;

  size_t num_files;
  size_t num_matches;

  struct Sample {
    std::filesystem::path file;
    uint32_t first_line, match_line;
    std::vector<std::string> lines;
  };

  std::vector<Sample> samples;
};

template <typename... Args>
CodeSearchResults make_error(Args&&... args) {
  CodeSearchResults results;
  results.error = dvc::concat(std::forward<Args>(args)...);
  return results;
}

constexpr size_t num_samples = 100;

inline CodeSearchResults codesearch(const std::filesystem::path& index_file,
                                    const std::string& query, size_t nthreads,
                                    size_t block_size) {
  DVC_ASSERT(exists(index_file), "No such file: ", index_file);
  mmapfile index_mmap(index_file);
  idx::IndexReader index(index_mmap.get());

  if (query.empty()) return make_error("Empty query string.");

  VectorTokenStream output;
  try {
    Tokenize(query, output);
  } catch (std::exception& e) {
    return make_error("Could not tokenize query string `", query,
                      "` because: ", e.what());
  }
  if (output.tokens.empty())
    return make_error("Query string contains no C++ tokens.");

  std::vector<std::byte> encoded(5 * (output.tokens.size() + 1));
  std::byte* ptr = encoded.data();
  for (const Token& token : output.tokens) {
    uint32_t token_id = index.token_id(token.spelling);
    if (token_id == 0)
      return make_error("No matches found.  (No such token in dataset `",
                        token.spelling, "`)");
    encode_token(token_id, ptr);
  }
  encoded.resize(ptr - encoded.data());
  DVC_ASSERT_GT(encoded.size(), 0);

  static const std::byte* query_begin = encoded.data();
  static const std::byte* query_end = encoded.data() + encoded.size();
  const std::byte* code_section_end = index.code + index.code_length;

  dvc::sampler<const std::byte*, num_samples> matches;

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

  CodeSearchResults results;
  results.num_files = index.num_files;
  results.num_matches = matches.size();
  std::vector<const std::byte*> samples = matches.build_samples();
  for (const std::byte* sample : samples) {
    CodeSearchResults::Sample out_sample;
    idx::IndexReader::FileLines file_lines =
        index.symbolize(sample, encoded.size(), 2);
    out_sample.file = index.filename(file_lines.file_info);
    out_sample.first_line = file_lines.first_lineno;
    out_sample.match_line = file_lines.match_lineno;
    dvc::file_reader file(out_sample.file);
    if (file_lines.num_lines > 0) file.seek(file_lines.lines[0].file_offset);
    for (uint32_t i = 0; i < file_lines.num_lines; i++) {
      out_sample.lines.push_back(
          file.read_string(file_lines.lines[i + 1].file_offset -
                           file_lines.lines[i].file_offset));
      std::string& s = out_sample.lines.back();
      if (!s.empty() && s.back() == '\n') s = s.substr(0, s.size() - 1);
    }
    results.samples.push_back(std::move(out_sample));
  }
  return results;
}

}  // namespace ppt
