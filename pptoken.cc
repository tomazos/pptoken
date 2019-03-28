#include <algorithm>
#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>
#include <thread>

#include "dvc/file.h"
#include "dvc/opts.h"
#include "dvc/program.h"
#include "dvc/string.h"
#include "tokenize.h"
#include "vector_token_stream.h"

namespace ppt {

std::filesystem::path DVC_OPTION(index_file, -, dvc::required, "index file");
std::filesystem::path DVC_OPTION(tokensout, -, dvc::required,
                                 "output token file");

std::atomic_size_t n = 0;
std::atomic_size_t totlines = 0;

std::mutex mu;

std::map<std::string, size_t> errors;

std::map<std::string, size_t> token_map;

void on_token(const Token& token) { token_map[token.spelling]++; }

void parse(size_t /*fileindex*/, const std::string& /*relpath*/,
           const std::filesystem::path& path) {
  std::string input = dvc::load_file(path);

  VectorTokenStream output;

  try {
    Tokenize(input, output);
  } catch (std::exception& e) {
    return;
  }

  for (const Token& token : output.tokens) {
    on_token(token);
  }
}

void pptoken(int argc, char** argv) {
  dvc::program program(argc, argv);

  dvc::file_writer tokensout(ppt::tokensout, dvc::truncate);

  DVC_ASSERT(exists(index_file), "File not found: ", index_file);

  std::vector<std::string> index = dvc::split("\n", dvc::load_file(index_file));
  if (index.back().empty()) index.pop_back();

  DVC_LOG("Number of files: ", index.size());
  for (const std::string relpath : index) {
    std::filesystem::path path = "/opt/" + relpath;
    DVC_ASSERT(exists(path), "File not found: ", path);
  }
  DVC_LOG("All files exist");

  std::vector<std::thread> threads;
  for (size_t i = 0; i < 20; i++)
    threads.emplace_back([&, i] {
      for (size_t j = 0; j < index.size(); j++)
        if (j % 20 == i) {
          size_t n = ppt::n++;
          parse(j, index[j], "/opt/" + index[j]);
          if ((n & (n - 1)) == 0) DVC_LOG("Processed ", n, " files");
        }
    });

  for (std::thread& t : threads) t.join();

  DVC_LOG("Processed ALL ", n, " files");

  std::vector<std::string> spellings;
  for (const auto& [k, v] : token_map) spellings.push_back(k);

  std::stable_sort(spellings.begin(), spellings.end(),
                   [&](const std::string& a, const std::string& b) {
                     return token_map.at(a) > token_map.at(b);
                   });

  tokensout.println(spellings.size());
  for (const std::string& spelling : spellings) {
    tokensout.println(token_map.at(spelling), " ", spelling.size(), " ",
                      spelling);
  }
}

}  // namespace ppt

int main(int argc, char** argv) { ppt::pptoken(argc, argv); }
