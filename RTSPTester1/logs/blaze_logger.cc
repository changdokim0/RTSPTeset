#include "blaze_logger.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <fstream>
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/stat.h>
#endif

BlazeLogger& BlazeLogger::GetInstance() {
  static BlazeLogger instance;
  return instance;
}

BlazeLogger::BlazeLogger() {
  try {
    fwrite_sinks::color_mode mode_ = fwrite_sinks::color_mode::never;
#ifdef _WIN32
    char env_blazelog[256] = {0};
    DWORD len = GetEnvironmentVariableA("BLAZE_LOG", env_blazelog, sizeof(env_blazelog));
    if (len > 0) {
      if (std::string(env_blazelog) == "console") {
        mode_ = fwrite_sinks::color_mode::always;
      }
    }
#else
    const char* env_blazelog = std::getenv("BLAZE_LOG");
    if (env_blazelog != nullptr) {
      if (std::string(env_blazelog) == "console") {
        mode_ = fwrite_sinks::color_mode::always;
      }
    }
#endif

    auto sink = std::make_shared<fwrite_sinks::fwrite_color_sink_mt>(stdout, false, mode_);
    sink->set_level(default_level_);
    logger_ = std::make_shared<spdlog::logger>("BlazeLogger", sink);
    SetPattern(default_pattern_);
    logger_->set_level(spdlog::level::trace);
    SetBacktraceSize(backtrace_buffer_size_);
  } catch (const std::exception& e) {
    fprintf(stdout, "[%s:%d] Exception caught in BlazeLogger constructor: %s \n", __func__, __LINE__, e.what());
  }
}

BlazeLogger ::~BlazeLogger() {}

std::string BlazeLogger::GetLogLevel() {
  std::string log_level = "";
  spdlog::level::level_enum sink_level = spdlog::level::trace;
  for (auto& sink : logger_->sinks()) {
    sink_level = sink->level();
  }
  switch (sink_level) {
    case spdlog::level::trace:
      log_level = "trace";
      break;
    case spdlog::level::debug:
      log_level = "debug";
      break;
    case spdlog::level::info:
      log_level = "info";
      break;
    case spdlog::level::warn:
      log_level = "warn";
      break;
    case spdlog::level::err:
      log_level = "error";
      break;
    case spdlog::level::off:
      log_level = "off";
      break;
    default:
      log_level = "info";
      break;
  }
  return log_level;
}

bool BlazeLogger::SetLogLevel(std::string level_str) {
  spdlog::level::level_enum target_level = spdlog::level::trace;
  if (level_str == "trace") {
    target_level = spdlog::level::trace;
  } else if (level_str == "debug") {
    target_level = spdlog::level::debug;
  } else if (level_str == "info") {
    target_level = spdlog::level::info;
  } else if (level_str == "warn") {
    target_level = spdlog::level::warn;
  } else if (level_str == "error") {
    target_level = spdlog::level::err;
  } else if (level_str == "off") {
    target_level = spdlog::level::off;
  } else {
    return false;
  }
  for (auto& sink : logger_->sinks()) {
    fprintf(stdout, "Setting to log level [%s]\n", level_str.c_str());
    sink->set_level(target_level);
  }
  return true;
}

void BlazeLogger::SetBacktraceSize(unsigned int num) {
  logger_->enable_backtrace(num);
}

void BlazeLogger::FlushBacktrace() {
  auto old_level = GetLogLevel();
  SetLogLevel("trace");
  logger_->dump_backtrace();
  SetLogLevel(old_level);
}

bool BlazeLogger::SetPattern(std::string pattern) {
  try {
    current_pattern_ = pattern;
    logger_->set_pattern(pattern);
  } catch (const std::exception& e) {
    fprintf(stdout, "[%s:%d] Exception caught in SetPattern: %s \n", __func__, __LINE__, e.what());
    return false;
  }
  return true;
}

std::shared_ptr<spdlog::logger> BlazeLogger::GetLogger() {
  return logger_;
}
