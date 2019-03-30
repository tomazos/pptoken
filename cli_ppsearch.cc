#include "ppsearch.h"

#include "dvc/opts.h"
#include "dvc/program.h"

namespace ppt {

std::filesystem::path DVC_OPTION(index_file, -, dvc::required,
                                 "input index file");

std::string DVC_OPTION(query, q, dvc::required, "query");

size_t DVC_OPTION(nthreads, n, dvc::required, "number of threads");

size_t DVC_OPTION(block_size, b, dvc::required, "number of blocks");

void ppsearch(int argc, char** argv) {
  dvc::program program(argc, argv);

  CodeSearchResults results =
      codesearch(index_file, query, nthreads, block_size);
  if (!results.error.empty()) DVC_FAIL(results.error);

  for (size_t i = 0; i < results.samples.size(); i++) {
    DVC_DUMP(i);
    DVC_DUMP(results.samples[i].file);
    DVC_DUMP(results.samples[i].first_line);
    DVC_DUMP(results.samples[i].match_line);
    for (const std::string& line : results.samples[i].lines) {
      DVC_LOG("line: `", line, "`");
    }
  }
  DVC_DUMP(results.num_files);
  DVC_DUMP(results.num_matches);
}

}  // namespace ppt

int main(int argc, char** argv) { ppt::ppsearch(argc, argv); }
