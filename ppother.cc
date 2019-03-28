#include <algorithm>
#include <unordered_map>

#include "dvc/file.h"
#include "dvc/opts.h"
#include "dvc/program.h"
#include "dvc/string.h"
#include "tokenize.h"
#include "vector_token_stream.h"

namespace ppt {

template <typename T>
void rwrite(T t) {
  std::cout.write((char*)&t, sizeof(T));
}

std::filesystem::path DVC_OPTION(index_path, -, dvc::required, "INDEX file");
std::filesystem::path DVC_OPTION(tokens_path, -, dvc::required, "TOKENS file");
std::filesystem::path DVC_OPTION(code_path, -, dvc::required, "CODE file");
std::filesystem::path DVC_OPTION(output_path, o, dvc::required, "OUTPUT file");

std::unordered_map<std::string, uint32_t> spelling_map;
std::vector<const std::string*> inv_spelling_vec;

size_t search(const std::vector<uint32_t>& query,
              const std::vector<uint32_t>& file) {
  if (query.size() > file.size()) return 0;
  size_t hits = 0;
  size_t n = file.size() - query.size() + 1;
  size_t m = query.size();
  for (size_t i = 0; i < n; i++) {
    bool hit = true;
    for (size_t j = 0; j < m; j++)
      if (file[i + j] != query[j]) {
        hit = false;
        break;
      }
    if (hit) hits++;
  }
  return hits;
}

std::unordered_map<uint32_t, size_t> hist;

void scan(std::vector<uint32_t> file) {
  for (size_t i = 0; i + 1 < file.size(); i++) {
    uint32_t a = file[i];
    uint32_t b = file[i + 1];
    if (a >= (1u << 16) || b >= (1u << 16)) continue;
    uint32_t c = (a << 16) | b;
    hist[c]++;
  }
}

void ppother(int argc, char** argv) {
  dvc::program program(argc, argv);

  DVC_ASSERT(exists(index_path));

  DVC_ASSERT(exists(tokens_path));

  DVC_ASSERT(exists(code_path));

  DVC_LOG("Loading TOKENS...");
  dvc::file_reader tokensin(tokens_path);

  uint32_t numentries;
  tokensin.istream() >> numentries;
  for (size_t i = 0; i < numentries; i++) {
    size_t occurences;
    tokensin.istream() >> occurences;
    DVC_ASSERT_EQ(tokensin.istream().get(), ' ');
    size_t spelling_len;
    tokensin.istream() >> spelling_len;
    DVC_ASSERT_EQ(tokensin.istream().get(), ' ');
    std::string spelling(spelling_len, '\0');
    tokensin.read(spelling.data(), spelling_len);
    DVC_ASSERT_EQ(tokensin.istream().get(), '\n');
    spelling_map[spelling] = i;
    if ((i & (i - 1)) == 0) DVC_LOG("Parsed spelling ", i, " of ", numentries);
  }
  inv_spelling_vec.resize(numentries);
  for (const auto& [k, v] : spelling_map) {
    inv_spelling_vec[v] = &k;
  }
  DVC_LOG("TOKENS loaded");

  DVC_LOG("Loading INDEX...");
  std::vector<std::string> index;

  for (std::filesystem::path p : dvc::split("\n", dvc::load_file(index_path))) {
    if (p.empty()) break;
    index.push_back(p);
  }
  DVC_LOG("INDEX loaded");

  dvc::file_reader code(code_path);

  DVC_LOG("Scanning CODE...");
  size_t nfiles = code.vread();
  DVC_ASSERT_EQ(nfiles, index.size());
  for (size_t i = 0; i < nfiles; i++) {
    if ((i & (i - 1)) == 0) {
      DVC_LOG("Scanning ", i, " of ", nfiles, " files.");
      DVC_DUMP(hist.size());
      std::vector<std::pair<uint32_t, uint32_t>> vec;
      for (const auto& [k, v] : hist) {
        vec.push_back(std::pair{k, v});
      }
      std::sort(vec.begin(), vec.end(),
                [](auto a, auto b) { return a.second > b.second; });
      for (size_t i = 0; i < std::min(vec.size(), size_t(10)); i++) {
        uint16_t a = vec[i].first >> 16;
        uint16_t b = vec[i].first & ((1u << 16) - 1u);
        std::cout << vec[i].second << " " << *inv_spelling_vec.at(a) << " "
                  << *inv_spelling_vec.at(b) << std::endl;
      }
    }
    size_t filelen = code.vread();
    size_t startpos = code.tell();
    std::vector<uint32_t> file;
    while (code.tell() < startpos + filelen) {
      file.push_back(code.vread());
    }
    DVC_ASSERT_EQ(code.tell(), startpos + filelen);

    scan(file);
  }

  std::vector<std::pair<uint32_t, uint32_t>> vec;
  for (const auto& [k, v] : hist) {
    vec.push_back(std::pair{k, v});
  }
  std::sort(vec.begin(), vec.end(),
            [](auto a, auto b) { return a.second > b.second; });

  dvc::file_writer output(output_path, dvc::truncate);
  for (size_t i = 0; i < vec.size(); i++) {
    uint16_t a = vec[i].first >> 16;
    uint16_t b = vec[i].first & ((1u << 16) - 1u);
    output.println(vec[i].second, " ", *inv_spelling_vec.at(a), " ",
                   *inv_spelling_vec.at(b));
  }
}

}  // namespace ppt

int main(int argc, char** argv) try {
  ppt::ppother(argc, argv);
} catch (std::exception& e) {
  std::cerr << "ERROR: " << e.what() << std::endl;
  return EXIT_FAILURE;
}
