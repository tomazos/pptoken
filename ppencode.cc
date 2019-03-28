#include <atomic>
#include <map>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "dvc/file.h"
#include "dvc/opts.h"
#include "dvc/program.h"
#include "dvc/string.h"
#include "tokenize.h"
#include "vector_token_stream.h"

namespace ppt {

std::filesystem::path DVC_OPTION(index_path, -, dvc::required, "index file");
std::filesystem::path DVC_OPTION(tokensin_path, -, dvc::required,
                                 "input token file");
std::filesystem::path DVC_OPTION(codeout_path, -, dvc::required,
                                 "output encoded file");

std::atomic_size_t n = 0;
std::atomic_size_t totbytes = 0;

std::mutex mu;

std::map<std::string, size_t> errors;

std::unordered_map<std::string, size_t> spelling_map;

std::vector<std::string> encoded;

void vwrite(std::ostream& o, size_t x) {
  while (x > 127u) {
    o.put(uint8_t(128u | (x & 127u)));
    x >>= 7;
  }
  o.put(uint8_t(x & 127u));
}

size_t vread(std::istream& i) {
  size_t x = 0;
  for (int p = 0; true; p += 7) {
    uint8_t c = i.get();
    x |= size_t(c & 127u) << p;
    if (!(c & 128u)) return x;
  }
}

std::string parse(size_t /*fileindex*/, const std::string& /*relpath*/,
                  const std::filesystem::path& path) {
  std::ostringstream o;
  std::string input = dvc::load_file(path);

  VectorTokenStream output;

  try {
    Tokenize(input, output);
  } catch (std::exception& e) {
    return "";
  }

  for (const Token& token : output.tokens)
    vwrite(o, spelling_map.at(token.spelling));
  return o.str();
}

void ppencode(int argc, char** argv) {
  dvc::program program(argc, argv);

  dvc::file_reader tokensin(tokensin_path);

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

  DVC_ASSERT(exists(index_path), "File not found: ", index_path);

  std::vector<std::string> index = dvc::split("\n", dvc::load_file(index_path));
  if (index.back().empty()) index.pop_back();

  DVC_LOG("Number of files: ", index.size());
  for (const std::string relpath : index) {
    std::filesystem::path path = "/opt/" + relpath;
    DVC_ASSERT(exists(path), "File not found: ", path);
  }
  DVC_LOG("All files exist");

  encoded.resize(index.size());

  std::vector<std::thread> threads;
  for (size_t i = 0; i < 20; i++)
    threads.emplace_back([&, i] {
      for (size_t j = 0; j < index.size(); j++)
        if (j % 20 == i) {
          size_t n = ppt::n++;
          encoded[j] = parse(j, index[j], "/opt/" + index[j]);
          if ((n & (n - 1)) == 0) DVC_LOG("Processed ", n, " files");
        }
    });

  for (std::thread& t : threads) t.join();

  DVC_LOG("Processed ALL ", n, " files");

  dvc::file_writer codeout(codeout_path, dvc::truncate);

  codeout.vwrite(index.size());
  for (size_t i = 0; i < index.size(); i++) {
    codeout.vwrite(encoded[i].size());
    codeout.write(encoded[i].data(), encoded[i].size());
  }
}

}  // namespace ppt

int main(int argc, char** argv) { ppt::ppencode(argc, argv); }
