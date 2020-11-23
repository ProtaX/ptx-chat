#ifndef CLIENT_LOG_H_
#define CLIENT_LOG_H_

#include <memory>
#include <unistd.h>
#include <string>

#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"

namespace ptxchat {

inline const char* LOG_FILENAME = "ptx_client.log";

inline std::shared_ptr<spdlog::logger> logger_;

inline void InitRotatingLogger(const std::string& name) {
  char cwd[256];
  getcwd(cwd, 256);
  size_t cwd_len = strlen(cwd);
  cwd[cwd_len] = '/';
  cwd[cwd_len + 1] = '\0';
  std::string full_path;
  if (name[0] != '/')
    full_path = std::string(cwd) + LOG_FILENAME;
  else
    full_path = LOG_FILENAME;
  logger_ = spdlog::rotating_logger_mt(name, full_path, 10000000, 10);
}

} // namespace ptxchat

#endif // CLIENT_LOG_H_
