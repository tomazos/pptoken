#include "ppsearch.h"

#include "dvc/terminate.h"

extern "C" {
#include <cgic.h>
}

int cgiMain() {
  cgiHeaderContentType((char*)"text/html");
  dvc::install_segfault_handler();
  dvc::install_terminate_handler();

  std::filesystem::path index_file = "/opt/actcd19.idx";
  size_t nthreads = 24;
  size_t block_size = 100000;

  fprintf(cgiOut, R"(
   <html>
     <head>
       <title>codesearch.isocpp.org</title>
     </head>
     <body>
       <p><b>codesearch.isocpp.org</b> by <a href="http://www.tomazos.com">Andrew Tomazos</a>.</p>
       <p>Enter a valid C/C++ code snippet...</p>
       <form method="GET" enctype="multipart/form-data" action=")");
  cgiValueEscape(cgiScriptName);
  fprintf(cgiOut, R"(">
         <input type="text" name="q" />
         <input type="submit" name="search" value="Search" />
       </form>)");

  if (cgiFormSubmitClicked((char*)"search") == cgiFormSuccess) {
    char query[256];
    cgiFormStringNoNewlines((char*)"q", query, 256);
    fprintf(cgiOut, "<p>Searching for <code>`");
    cgiValueEscape(query);
    fprintf(cgiOut, "`</code>...</p>\n");

    ppt::CodeSearchResults results =
        ppt::codesearch(index_file, query, nthreads, block_size);

    if (!results.error.empty()) {
      fprintf(cgiOut, "<p><b>");
      cgiValueEscape((char*)results.error.c_str());
      fprintf(cgiOut, "</b></p>");
    } else {
      fprintf(cgiOut,
              "<p>%lu source files searched.</p><p><b>%lu matches</b> "
              "found.</p><p>Here is a random sample of matches...</p>",
              results.num_files, results.num_matches);

      for (size_t i = 0; i < results.samples.size(); i++) {
        std::string relpath = results.samples[i].file.string().substr(5);
        std::string url = "http://codesearch.isocpp.org/" + relpath;
        fprintf(cgiOut, "<hr/><p><pre><a href =\"");
        cgiValueEscape((char*)url.c_str());
        fprintf(cgiOut, "\">");
        cgiValueEscape((char*)relpath.c_str());
        fprintf(cgiOut, "</a>:%u:</pre></p>\n", results.samples[i].match_line);

        fprintf(cgiOut, "<p><pre>");

        // uint32_t line = results.samples[i].first_line;
        for (const std::string& line : results.samples[i].lines) {
          fprintf(cgiOut, "    ");
          cgiValueEscape((char*)line.c_str());
          fprintf(cgiOut, "<br/>");
        }
        fprintf(cgiOut, "</pre></p>\n");
      }
    }
  }

  fprintf(cgiOut, R"(
    <body>
  </html>
)");
  return 0;
}
