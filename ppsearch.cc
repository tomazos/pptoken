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

std::filesystem::path DVC_OPTION(index_path, -, "/opt/actcd19/INDEX",
                                 "INDEX file");
std::filesystem::path DVC_OPTION(tokens_path, -, "/opt/actcd19/TOKENS",
                                 "TOKENS file");
std::filesystem::path DVC_OPTION(code_path, -, "/opt/actcd19/CODE",
                                 "CODE file");
std::string DVC_OPTION(query, q, "", "QUERY");
std::filesystem::path DVC_OPTION(query_path, -, "", "QUERY file");
std::filesystem::path DVC_OPTION(result_path, o, dvc::required, "RESULT file");
size_t DVC_OPTION(match_limit, -, std::numeric_limits<size_t>::max(),
                  "max matches");

std::unordered_map<std::string, size_t> spelling_map;

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

void ppsearch(int argc, char** argv) {
  dvc::program program(argc, argv);

  DVC_ASSERT(exists(index_path));

  DVC_ASSERT(exists(tokens_path));

  DVC_ASSERT(exists(code_path));

  DVC_LOG("Parsing QUERY...");

  DVC_ASSERT((!exists(query_path) && !query.empty()) ||
             (exists(query_path) && query.empty()));

  std::string input = (query.empty() ? dvc::load_file(query_path) : query);

  VectorTokenStream query_stream;

  Tokenize(input, query_stream);

  std::vector<Token> query_tokens = query_stream.tokens;

  DVC_LOG("Loading TOKENS...");
  dvc::file_reader tokensin(tokens_path);

  size_t numentries;
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
  DVC_LOG("TOKENS loaded");

  std::vector<uint32_t> query;

  for (const Token& token : query_tokens) {
    auto it = spelling_map.find(token.spelling);
    if (it == spelling_map.end()) {
      std::cout << "No hits" << std::endl;
      return;
    }
    query.push_back(it->second);
  }

  DVC_LOG("Loading INDEX...");
  std::vector<std::string> index;

  for (std::filesystem::path p : dvc::split("\n", dvc::load_file(index_path))) {
    if (p.empty()) break;
    index.push_back(p);
  }
  DVC_LOG("INDEX loaded");

  dvc::file_reader code(code_path);

  dvc::file_writer result(result_path, dvc::truncate);

  DVC_LOG("Searching CODE...");
  size_t nfiles = code.vread();
  DVC_ASSERT_EQ(nfiles, index.size());
  size_t total_hits = 0;
  for (size_t i = 0; i < nfiles; i++) {
    if ((i & (i - 1)) == 0)
      DVC_LOG("Searched ", i, " of ", nfiles, " files: ", total_hits,
              " hits so far.");
    size_t filelen = code.vread();
    size_t startpos = code.tell();
    std::vector<uint32_t> file;
    while (code.tell() < startpos + filelen) {
      file.push_back(code.vread());
    }
    DVC_ASSERT_EQ(code.tell(), startpos + filelen);
    size_t hits = search(query, file);
    if (hits > 0) result.println(hits, " \"", index.at(i), "\"");
    total_hits += hits;
    if (total_hits > match_limit) {
      DVC_ERROR("hit limit reached: ", i, " files searched with ", total_hits,
                "hits");
      return;
    }
  }
}

}  // namespace ppt

int main(int argc, char** argv) try {
  ppt::ppsearch(argc, argv);
} catch (std::exception& e) {
  std::cerr << "ERROR: " << e.what() << std::endl;
  return EXIT_FAILURE;
}
