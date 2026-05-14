#pragma once
#include <string>
#include <memory>
#include "spdlog/spdlog.h"
#include <thread>
#include <atomic>
#include <filesystem>
#include <exception>

#include <spdlog/sinks/base_sink.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/details/os.h>
#include <spdlog/fmt/bundled/format.h>
#include <cstdio>
#include <mutex>

namespace fwrite_sinks {

// Simple ANSI palette (used only if color is enabled)
struct ansi_palette {
  const char* reset = "\033[0m";
  const char* trace = "\033[37m";       // gray
  const char* debug = "\033[36m";       // cyan
  const char* info = "\033[32m";        // green
  const char* warn = "\033[33m";        // yellow
  const char* err = "\033[31m";         // red
  const char* critical = "\033[1;41m";  // bold + red background
  const char* off = "";
};

inline const char* color_for(spdlog::level::level_enum lvl, const ansi_palette& p) {
  switch (lvl) {
    case spdlog::level::trace:
      return p.trace;
    case spdlog::level::debug:
      return p.debug;
    case spdlog::level::info:
      return p.info;
    case spdlog::level::warn:
      return p.warn;
    case spdlog::level::err:
      return p.err;
    case spdlog::level::critical:
      return p.critical;
    default:
      return p.off;
  }
}

enum class color_mode {
  never,  // never output ANSI color
  always  // always wrap line with ANSI color according to level
};

template <typename Mutex>
class fwrite_color_sink final : public spdlog::sinks::base_sink<Mutex> {
 public:
  // If close_on_destroy=true, fclose(file_) in destructor
  explicit fwrite_color_sink(FILE* file = stdout, bool close_on_destroy = false, color_mode mode = color_mode::never)
      : file_(file), close_on_destroy_(close_on_destroy), mode_(mode) {}

  ~fwrite_color_sink() override {
    if (close_on_destroy_ && file_) {
      std::fclose(file_);
      file_ = nullptr;
    }
  }

  // Enable/disable color at runtime
  void set_color_mode(color_mode m) {
    std::lock_guard<Mutex> lock(this->mutex_);
    mode_ = m;
  }

  // Override palette if desired
  void set_palette(const ansi_palette& pal) {
    std::lock_guard<Mutex> lock(this->mutex_);
    palette_ = pal;
  }

 protected:
  void sink_it_(const spdlog::details::log_msg& msg) override {
    spdlog::memory_buf_t buf;
    this->formatter_->format(msg, buf);

    const char* data = buf.data();
    size_t size = buf.size();

    if (mode_ == color_mode::always) {
      const char* c = color_for(msg.level, palette_);
      // Color prefix
      if (*c)
        std::fwrite(c, 1, std::strlen(c), file_);
      // Message
      std::fwrite(data, 1, size, file_);
      // Reset suffix
      if (*c)
        std::fwrite(palette_.reset, 1, std::strlen(palette_.reset), file_);
    } else {
      // Plain text (no ANSI)
      std::fwrite(data, 1, size, file_);
    }
  }

  void flush_() override { std::fflush(file_); }

 private:
  FILE* file_{stdout};
  bool close_on_destroy_{false};
  color_mode mode_{color_mode::never};
  ansi_palette palette_{};
};

// Aliases
using fwrite_color_sink_st = fwrite_color_sink<spdlog::details::null_mutex>;
using fwrite_color_sink_mt = fwrite_color_sink<std::mutex>;

}  // namespace fwrite_sinks

namespace fmt {
/**
 * @brief Generic formatter specialization for enum types.
 *
 * Required when using spdlog with fmt 10.0.0 or later.
 * This formatter converts any enum type to its underlying integer
 * representation so it can be safely formatted by fmt/spdlog.
 
 * Example:
 *   enum class Color : uint8_t { Red = 1, Green = 2, Blue = 3 };
 *   SPDLOG_ERROR("Color: {}\n", Color::Green); // logs the integer value, e.g., "2"
 */
template <typename Enum>
struct formatter<Enum, char, typename std::enable_if<std::is_enum<Enum>::value>::type> : formatter<typename std::underlying_type<Enum>::type, char> {

  // **must be const**
  template <typename FormatContext>
  auto format(const Enum e, FormatContext& ctx) const {
    using Underlying = typename std::underlying_type<Enum>::type;
    return formatter<Underlying, char>::format(static_cast<Underlying>(e), ctx);
  }
};
}  // namespace fmt

#ifdef _WIN32
#ifdef BLAZE_LOGGER_EXPORTS
#define BLAZE_LOGGER_API __declspec(dllexport)
#else
#define BLAZE_LOGGER_API __declspec(dllimport)
#endif
#else
#define BLAZE_LOGGER_API
#endif
#ifdef APPLICATION_NAME
#define FILE_LOCATION APPLICATION_NAME "::" __FILE__
#define BLAZE_LOG_DEBUG(fmt, ...) BlazeLogger::GetInstance().Log(spdlog::level::debug, FILE_LOCATION, __LINE__, fmt, ##__VA_ARGS__)
#define BLAZE_LOG_INFO(fmt, ...) BlazeLogger::GetInstance().Log(spdlog::level::info, FILE_LOCATION, __LINE__, fmt, ##__VA_ARGS__)                                 
#define BLAZE_LOG_WARN(fmt, ...) BlazeLogger::GetInstance().Log(spdlog::level::warn, FILE_LOCATION, __LINE__, fmt, ##__VA_ARGS__)
#define BLAZE_LOG_ERROR(fmt, ...) BlazeLogger::GetInstance().Log(spdlog::level::err, FILE_LOCATION, __LINE__, fmt, ##__VA_ARGS__)
#define BLAZE_LOG_TRACE(fmt, ...) BlazeLogger::GetInstance().Log(spdlog::level::trace, FILE_LOCATION, __LINE__, fmt, ##__VA_ARGS__)
#else
#define BLAZE_LOG_DEBUG(fmt, ...) BlazeLogger::GetInstance().Log(spdlog::level::debug, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define BLAZE_LOG_INFO(fmt, ...) BlazeLogger::GetInstance().Log(spdlog::level::info, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define BLAZE_LOG_WARN(fmt, ...) BlazeLogger::GetInstance().Log(spdlog::level::warn, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define BLAZE_LOG_ERROR(fmt, ...) BlazeLogger::GetInstance().Log(spdlog::level::err, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define BLAZE_LOG_TRACE(fmt, ...) BlazeLogger::GetInstance().Log(spdlog::level::trace, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#endif
#define BLAZE_LOG_FLUSH_BACKTRACE(...) BlazeLogger::GetInstance().FlushBacktrace()
class BLAZE_LOGGER_API BlazeLogger {
 public:
  static BlazeLogger& GetInstance();

  template <typename... Args>
  void Log(spdlog::level::level_enum level, const char* file, int line, const char* fmt, const Args&... args) {
    if (nullptr == logger_)
      return;
    try {
      spdlog::source_loc location(file, line, SPDLOG_FUNCTION);
      logger_->log(location, level, fmt::runtime(fmt), args...);
    } catch (const std::exception& e) {
      std::printf("Logging error: %s\n", e.what());
    } catch (...) {
      std::printf("Logging error: unknown exception\n");
    }
  }

  std::string GetLogLevel();
  bool SetLogLevel(std::string level_str);
  bool SetPattern(std::string pattern);
  void SetBacktraceSize(unsigned int num);
  void FlushBacktrace();
  std::shared_ptr<spdlog::logger> GetLogger();
 private:
  BlazeLogger();
  ~BlazeLogger();

 private:
  std::shared_ptr<spdlog::logger> logger_ = nullptr;
  spdlog::level::level_enum default_level_ = spdlog::level::info;
  std::string default_pattern_ = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] (%t) [%s:%#] %v";
  std::string current_pattern_ = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] (%t) [%s:%#] %v";
  unsigned int backtrace_buffer_size_ = 1000;
};
