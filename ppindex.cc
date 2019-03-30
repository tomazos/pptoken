#include <algorithm>
#include <atomic>
#include <filesystem>
#include <map>
#include <mutex>
#include <random>
#include <thread>
#include <unordered_map>

#include "dvc/file.h"
#include "dvc/log.h"
#include "dvc/math.h"
#include "dvc/opts.h"
#include "dvc/program.h"
#include "dvc/sha3.h"
#include "index.h"
#include "token_codec.h"
#include "tokenize.h"
#include "vector_token_stream.h"

namespace ppt {

std::filesystem::path DVC_OPTION(srcdir, s, dvc::required,
                                 "input source directory");

std::filesystem::path DVC_OPTION(output_index, o, dvc::required,
                                 "output index file");

std::filesystem::path DVC_OPTION(output_skipped_files, -,
                                 std::filesystem::path{},
                                 "file to output skipped files to");

std::filesystem::path DVC_OPTION(output_token_counts, -,
                                 std::filesystem::path{},
                                 "file to output token counts to");

size_t DVC_OPTION(nthreads, -, 20, "number of threads");

size_t DVC_OPTION(max_file_size, -, 300000, "maximum source file size");

void ppindex(int argc, char** argv) {
  dvc::program program(argc, argv);

  DVC_ASSERT(exists(srcdir) && is_directory(srcdir),
             "No such directory: ", srcdir);

  std::optional<dvc::file_writer> skipped_files;

  if (!output_skipped_files.empty())
    skipped_files = dvc::file_writer(output_skipped_files, dvc::truncate);

  size_t nskipped_files = 0;

  std::map<std::string, size_t> skipped_reasons;
  auto skip_file = [&](const std::filesystem::path& skipped_file,
                       const std::string& reason) {
    nskipped_files++;
    if (skipped_files) skipped_files->println(skipped_file.string());
    skipped_reasons[reason]++;
  };

  std::vector<std::filesystem::path> files1;

  DVC_LOG("Pass 1: Analyzing ", srcdir, ".");
  for (const std::filesystem::directory_entry& dirent :
       std::filesystem::recursive_directory_iterator(srcdir)) {
    if (dirent.is_directory()) continue;

    DVC_ASSERT(dirent.is_regular_file(), "Irregular file: ", dirent.path());

    if (dirent.file_size() > max_file_size) {
      skip_file(dirent, "too large");
      continue;
    }

    files1.push_back(dirent);

    if (dvc::is_pow2(files1.size())) {
      DVC_LOG("Analyzed ", files1.size(), " source files.");
    }
  }
  DVC_LOG("Pass 1 of ", srcdir, " complete: ", files1.size(),
          " source files analyzed.");
  if (nskipped_files > 0) {
    DVC_ERROR(nskipped_files, " files were skipped for the following reasons:");
    for (const auto& [reason, skipped] : skipped_reasons) {
      DVC_ERROR(skipped, " files were skipped because: ", reason);
    }
  }

  std::vector<std::filesystem::path> files2;
  std::map<std::string, size_t> token_map;

  DVC_LOG("Pass 2: Analyzing ", srcdir, ".");
  std::vector<std::thread> threads;
  std::mutex mu;
  std::atomic_size_t files_processed = 0;
  std::atomic_size_t num_tokens = 0;
  for (size_t thread_index = 0; thread_index < nthreads; thread_index++)
    threads.emplace_back([&, thread_index] {
      for (size_t files1_index = 0; files1_index < files1.size();
           files1_index++)
        if (files1_index % nthreads == thread_index) {
          std::string code = dvc::load_file(files1[files1_index]);

          std::vector<Token> tokens;

          try {
            VectorTokenStream output;
            Tokenize(code, output);
            tokens = std::move(output.tokens);
            std::lock_guard lock(mu);
            files2.push_back(files1[files1_index]);
            num_tokens += tokens.size();
            for (const Token& token : tokens) token_map[token.spelling]++;
          } catch (std::exception& e) {
            std::lock_guard lock(mu);
            skip_file(files1[files1_index], e.what());
            continue;
          }
          if (dvc::is_pow2(files_processed++))
            DVC_LOG("Analyzed ", files_processed, " files.");
        }
    });

  for (std::thread& t : threads) t.join();
  threads.clear();
  files1.clear();

  DVC_LOG("Pass 2 of ", srcdir, " complete: ", files2.size(),
          " source files analyzed.");

  if (nskipped_files > 0) {
    DVC_ERROR(nskipped_files, " files were skipped for the following reasons:");
    for (const auto& [reason, skipped] : skipped_reasons) {
      DVC_ERROR(skipped, " files were skipped because: ", reason);
    }
  }

  DVC_LOG("Num tokens: ", num_tokens);
  DVC_LOG("Unique tokens: ", token_map.size());
  DVC_LOG("Inverting token counts...");
  std::multimap<size_t, const std::string*> count_to_token;
  for (const auto& [k, v] : token_map) count_to_token.insert(std::pair(v, &k));

  std::vector<const std::string*> inv_token_vec;

  std::string eof_token;

  inv_token_vec.push_back(&eof_token);

  for (auto it = count_to_token.crbegin(); it != count_to_token.crend(); it++) {
    inv_token_vec.push_back(it->second);
  }

  if (!output_token_counts.empty()) {
    dvc::file_writer token_counts_file(output_token_counts, dvc::truncate);
    for (auto it = count_to_token.crbegin(); it != count_to_token.crend();
         it++) {
      size_t count = it->first;
      const std::string& token = *(it->second);
      token_counts_file.println(count, " ", token.size(), " ", token);
    }
  }
  count_to_token.clear();

  for (size_t i = 1; i < inv_token_vec.size(); i++)
    token_map.at(*inv_token_vec[i]) = i;

  DVC_LOG("Shuffling file list...");
  std::mt19937 rand_engine;
  std::shuffle(files2.begin(), files2.end(), rand_engine);

  DVC_LOG("Pass 3: Analyzing ", srcdir, ".");
  files_processed = 0;
  std::atomic_size_t num_encoded_bytes = 0;
  std::atomic_size_t num_newlines = 0;
  std::unordered_map<std::string, size_t> shas;
  std::atomic_size_t total_tokens = 0;
  std::atomic_size_t total_bytes = 0;
  for (size_t thread_index = 0; thread_index < nthreads; thread_index++)
    threads.emplace_back([&, thread_index] {
      for (size_t files2_index = 0; files2_index < files2.size();
           files2_index++)
        if (files2_index % nthreads == thread_index) {
          std::string code = dvc::load_file(files2.at(files2_index));

          std::vector<std::byte> encoded;
          try {
            VectorTokenStream output;
            Tokenize(code, output);
            size_t newlines = output.newlines_tokens.size();
            encoded.resize(5 * (output.tokens.size() + 1));
            std::byte* ptr = encoded.data();
            for (const Token& token : output.tokens) {
              DVC_ASSERT(!token.spelling.empty());
              uint32_t token_id = token_map.at(token.spelling);
              encode_token(token_id, ptr);
            }
            encode_token(0, ptr);
            encoded.resize(ptr - encoded.data());
            DVC_ASSERT(uint8_t(encoded.back()) == 0);
            std::string sha =
                dvc::SHA3({(const char*)encoded.data(), encoded.size()});
            std::lock_guard lock(mu);
            auto it = shas.find(sha);
            if (!output.tokens.empty() && it == shas.end()) {
              shas.emplace_hint(it, std::move(sha), files2_index);
              num_encoded_bytes += encoded.size();
              num_newlines += newlines;
              total_tokens += output.tokens.size();
              total_bytes += code.size();
            } else {
              if (output.tokens.empty())
                skip_file(files2.at(files2_index), "no tokens");
              else
                skip_file(files2.at(files2_index), "same sha");
            }
          } catch (std::exception& e) {
            DVC_FATAL("Unexpected bad file: ", e.what(), ": ",
                      files2[files2_index]);
          }

          size_t n = files_processed++;
          if (dvc::is_pow2(n))
            DVC_LOG("Analyzed ", n,
                    " files. num_encoded_bytes = ", num_encoded_bytes);
        }
    });
  for (std::thread& t : threads) t.join();
  threads.clear();
  DVC_LOG("Pass 3 of ", srcdir, " complete: ", shas.size(),
          " source files analyzed.");

  if (nskipped_files > 0) {
    DVC_ERROR(nskipped_files, " files were skipped for the following reasons:");
    for (const auto& [reason, skipped] : skipped_reasons) {
      DVC_ERROR(skipped, " files were skipped because: ", reason);
    }
  }
  DVC_LOG("Number of encoded bytes: ", num_encoded_bytes);
  std::vector<std::filesystem::path> files;
  for (const auto& [k, v] : shas) files.push_back(files2[v]);
  shas.clear();
  files2.clear();

  DVC_LOG("Writing index...");
  dvc::file_writer index(output_index, dvc::truncate);

  DVC_LOG("Writing header...");
  idx::IndexHeader header;
  header.code_section_length = num_encoded_bytes;
  header.file_section_offset = sizeof(idx::IndexHeader);
  header.num_files = files.size();
  header.num_tokens = token_map.size();
  header.total_tokens = total_tokens;
  header.total_lines = num_newlines;
  header.total_bytes = total_bytes;
  index.rwrite(header);

  header.file_section_offset = index.tell();
  DVC_LOG("Padding file section @ ", header.file_section_offset);
  for (const std::filesystem::path& file : files) {
    (void)file;
    index.rwrite(idx::FileInfo{});
  }

  header.token_id_section_offset = index.tell();
  DVC_LOG("Padding token id section @ ", header.token_id_section_offset);
  for (size_t i = 0; i < header.num_tokens; i++)
    index.rwrite(idx::TokenIdInfo{});

  header.token_alphabetical_section_offset = index.tell();
  DVC_LOG("Writing token alphabetical section @ ",
          header.token_alphabetical_section_offset);
  for (const auto& [spelling, token_id] : token_map) {
    idx::TokenAlphabeticalInfo info;
    info.token_id = token_id;
    index.rwrite(info);
  }

  size_t lineinfo_offset = index.tell();
  DVC_LOG("Padding line info section @ ", lineinfo_offset);
  void* pad = std::malloc(num_newlines * sizeof(idx::LineInfo));
  DVC_ASSERT(pad);
  index.write(pad, num_newlines * sizeof(idx::LineInfo));
  std::free(pad);

  header.code_section_offset = index.tell();
  DVC_LOG("Padding code section @ ", header.code_section_offset);
  pad = std::malloc(num_encoded_bytes);
  DVC_ASSERT(pad);
  index.write(pad, num_encoded_bytes);
  std::free(pad);

  size_t filename_offset = index.tell();
  DVC_LOG("Writing filenames @ ", filename_offset);
  for (const std::filesystem::path& file : files) {
    std::string filename_cstr = file.string();
    index.write(filename_cstr.c_str(), filename_cstr.size() + 1);
  }

  size_t spelling_offset = index.tell();
  DVC_LOG("Writing spelling offset @ ", spelling_offset);
  for (size_t i = 1; i < inv_token_vec.size(); i++) {
    index.write(inv_token_vec[i]->c_str(), inv_token_vec[i]->size() + 1);
  }

  size_t total_index_size = index.tell();
  DVC_LOG("Total index size: ", total_index_size);

  DVC_LOG("Backpatching header:");
  index.seek(0);
  index.rwrite(header);

  DVC_LOG("Backpatching file info...");
  std::vector<idx::FileInfo> file_infos(files.size());
  size_t filename_cstr = filename_offset;
  for (size_t i = 0; i < files.size(); i++) {
    file_infos[i].filename_cstr = filename_cstr;
    filename_cstr += files[i].string().size() + 1;
  }

  DVC_LOG("Pass 4: Analyzing ", srcdir, ".");
  files_processed = 0;
  for (size_t thread_index = 0; thread_index < nthreads; thread_index++)
    threads.emplace_back([&, thread_index] {
      for (size_t file_index = 0; file_index < files.size(); file_index++)
        if (file_index % nthreads == thread_index) {
          idx::FileInfo& file_info = file_infos[file_index];

          std::string code = dvc::load_file(files[file_index]);
          file_info.file_length = code.size();

          std::vector<std::byte> encoded;
          try {
            VectorTokenStream output;
            Tokenize(code, output);
            file_info.num_lines = output.newlines_tokens.size();

            encoded.resize(5 * (output.tokens.size() + 1));
            std::byte* ptr = encoded.data();
            for (const Token& token : output.tokens) {
              DVC_ASSERT(!token.spelling.empty());
              uint32_t token_id = token_map.at(token.spelling);
              encode_token(token_id, ptr);
            }
            encode_token(0, ptr);
            encoded.resize(ptr - encoded.data());
            DVC_ASSERT(uint8_t(encoded.back()) == 0);
            file_info.code_length = encoded.size();
          } catch (std::exception& e) {
            DVC_FATAL("Unexpected bad file: ", e.what(), ": ",
                      files[file_index]);
          }

          if (dvc::is_pow2(files_processed++))
            DVC_LOG("Analyzed ", files_processed, " files.");
        }
    });
  for (std::thread& t : threads) t.join();
  threads.clear();
  DVC_LOG("Pass 4 of ", srcdir, " complete.");

  DVC_LOG("Backpatching file info...");
  index.seek(header.file_section_offset);
  size_t code_offset = 0;
  size_t current_lineinfo_offset = lineinfo_offset;
  for (idx::FileInfo& file_info : file_infos) {
    file_info.code_offset = code_offset;
    file_info.lineinfo_offset = current_lineinfo_offset;
    index.rwrite(file_info);

    code_offset += file_info.code_length;
    current_lineinfo_offset += (file_info.num_lines * sizeof(idx::LineInfo));
  }
  DVC_ASSERT_EQ(header.code_section_length, code_offset);
  DVC_ASSERT_EQ(header.code_section_offset, current_lineinfo_offset);

  DVC_LOG("Backpatching token id section @ ", header.token_id_section_offset);
  index.seek(header.token_id_section_offset);
  size_t spelling_cstr = spelling_offset;
  DVC_ASSERT_EQ(header.num_tokens + 1, inv_token_vec.size());
  for (size_t i = 0; i < header.num_tokens; i++) {
    idx::TokenIdInfo id;
    id.spelling_cstr = spelling_cstr;
    index.rwrite(id);
    spelling_cstr += inv_token_vec[i + 1]->size() + 1;
  }
  DVC_ASSERT_EQ(spelling_cstr, total_index_size);

  DVC_LOG("Backpatching code section @ ", header.code_section_offset);
  DVC_ASSERT_EQ(num_encoded_bytes, header.code_section_length);
  std::vector<std::byte> code_section(num_encoded_bytes);

  DVC_LOG("Pass 5: Analyzing ", srcdir, ".");
  files_processed = 0;
  for (size_t thread_index = 0; thread_index < nthreads; thread_index++)
    threads.emplace_back([&, thread_index] {
      for (size_t file_index = 0; file_index < files.size(); file_index++)
        if (file_index % nthreads == thread_index) {
          const idx::FileInfo& file_info = file_infos.at(file_index);

          std::string code = dvc::load_file(files[file_index]);

          std::byte* dest = code_section.data() + file_info.code_offset;
          try {
            VectorTokenStream output;
            Tokenize(code, output);
            for (const Token& token : output.tokens) {
              DVC_ASSERT(!token.spelling.empty());
              uint32_t token_id = token_map.at(token.spelling);
              encode_token(token_id, dest);
            }
            encode_token(0, dest);
            DVC_ASSERT_EQ(size_t(dest - code_section.data()),
                          file_info.code_offset + file_info.code_length);
          } catch (std::exception& e) {
            DVC_FATAL("Unexpected bad file: ", e.what(), ": ",
                      files[file_index]);
          }

          if (dvc::is_pow2(files_processed++))
            DVC_LOG("Analyzed ", files_processed, " files.");
        }
    });
  for (std::thread& t : threads) t.join();
  threads.clear();
  DVC_LOG("Pass 5 of ", srcdir, " complete.");

  DVC_LOG("Continuing backpatching code section @ ",
          header.code_section_offset);
  index.seek(header.code_section_offset);
  index.write(code_section.data(), code_section.size());
  code_section.clear();

  DVC_LOG("Backpatching line info section @ ", lineinfo_offset);
  index.seek(lineinfo_offset);

  std::vector<idx::LineInfo> line_infos(num_newlines);

  DVC_LOG("Pass 6: Analyzing ", srcdir, ".");
  files_processed = 0;
  for (size_t thread_index = 0; thread_index < nthreads; thread_index++)
    threads.emplace_back([&, thread_index] {
      for (size_t file_index = 0; file_index < files.size(); file_index++)
        if (file_index % nthreads == thread_index) {
          const idx::FileInfo& file_info = file_infos.at(file_index);
          idx::LineInfo* line_info =
              line_infos.data() +
              (file_info.lineinfo_offset - lineinfo_offset) /
                  sizeof(idx::LineInfo);

          std::string code = dvc::load_file(files[file_index]);

          try {
            VectorTokenStream output;
            Tokenize(code, output);
            size_t num_lines = output.newlines_tokens.size();
            DVC_ASSERT_EQ(num_lines, file_info.num_lines);
            for (size_t i = 0; i < num_lines; i++) {
              line_info[i].file_offset = output.newlines_tokens[i].file_offset;
            }
            std::vector<std::byte> encoded(5 * (output.tokens.size() + 1));
            std::byte* ptr = encoded.data();
            std::vector<uint32_t> code_offsets;
            for (const Token& token : output.tokens) {
              code_offsets.push_back(ptr - encoded.data());
              DVC_ASSERT(!token.spelling.empty());
              uint32_t token_id = token_map.at(token.spelling);
              encode_token(token_id, ptr);
            }
            code_offsets.push_back(ptr - encoded.data());
            DVC_ASSERT_EQ(code_offsets.size(), output.tokens.size() + 1);
            encode_token(0, ptr);
            for (size_t i = 0; i < num_lines; i++) {
              line_info[i].code_offset =
                  code_offsets.at(output.newlines_tokens[i].token_id);
            }
          } catch (std::exception& e) {
            DVC_FATAL("Unexpected bad file: ", e.what(), ": ",
                      files[file_index]);
          }

          if (dvc::is_pow2(files_processed++))
            DVC_LOG("Analyzed ", files_processed, " files.");
        }
    });
  for (std::thread& t : threads) t.join();
  threads.clear();
  DVC_LOG("Pass 6 of ", srcdir, " complete.");

  DVC_LOG("Continuing backpatching line info section @ ", lineinfo_offset);
  index.write(line_infos.data(), line_infos.size() * sizeof(idx::LineInfo));
  DVC_ASSERT_EQ(header.code_section_offset, index.tell());
}

}  // namespace ppt

int main(int argc, char** argv) { ppt::ppindex(argc, argv); }
