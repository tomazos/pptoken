#pragma once

#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <filesystem>
#include <string_view>
#include "dvc/log.h"

namespace ppt {

class mmapfile {
 public:
  mmapfile(const std::filesystem::path& path) : path(path) {
    DVC_ASSERT(exists(path), "File not found: ", path);
    fd = ::open(path.string().c_str(), O_RDONLY);
    DVC_ASSERT_NE(fd, -1, "Unable to open ", path, ": ", strerror(errno));
    length = file_size(path);
    addr = ::mmap(nullptr, length, PROT_READ, MAP_SHARED, fd, 0);
    DVC_ASSERT_NE(addr, MAP_FAILED, "Unable to mmap ", path, ": ",
                  strerror(errno));
    DVC_ASSERT_EQ(::mlock(addr, length), 0, "Unable to mlock ", path, ": ",
                  strerror(errno));
  }

  std::string_view get() { return {(const char*)addr, length}; }

  ~mmapfile() {
    DVC_ASSERT_EQ(::munlock(addr, length), 0, "Unable to mlock ", path, ": ",
                  strerror(errno));
    DVC_ASSERT_EQ(::munmap(addr, length), 0, "munmap ", path,
                  "failed: ", strerror(errno));
    DVC_ASSERT_EQ(::close(fd), 0, "close ", path,
                  " failed: ", ::strerror(errno));
  }

 private:
  std::filesystem::path path;
  int fd = -1;
  size_t length = 0;
  void* addr = nullptr;

  mmapfile(mmapfile&&) = delete;
  mmapfile(const mmapfile&) = delete;
};

}  // namespace ppt
