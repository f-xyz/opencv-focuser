#pragma once

#include "../utils/colors.h"
#include "../utils/utils.h"
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <print>
#include <string_view>
#include <utility>

class Logger {
  std::ofstream file;

public:
  explicit Logger(const std::filesystem::path &logFilePath) {
    file.open(logFilePath, std::ios_base::trunc);
  }

  template <typename... Args>
  void header(const std::format_string<Args...> &fmt, Args &&...args) {
    auto message = std::format(fmt, std::forward<Args>(args)...);
    printLine(bold(message));
    writeLine(message, "INFO");
  }

  template <typename... Args>
  void info(const std::format_string<Args...> &fmt, Args &&...args) {
    auto message = std::format(fmt, std::forward<Args>(args)...);
    printLine(message);
    writeLine(message, "INFO");
  }

  template <typename... Args>
  void debug(const std::format_string<Args...> &fmt, Args &&...args) {
    auto message = std::format(fmt, std::forward<Args>(args)...);
    writeLine(message, "DEBUG");
  }

  template <typename... Args>
  void error(const std::format_string<Args...> &fmt, Args &&...args) {
    auto message = std::format(fmt, std::forward<Args>(args)...);
    printLine(message, 128, 0, 0);
    writeLine(message, "ERROR");
  }

  template <typename... Args>
  void success(const std::format_string<Args...> &fmt, Args &&...args) {
    auto message = std::format(fmt, std::forward<Args>(args)...);
    printLine(message, 0, 128, 0);
    writeLine(message, "INFO");
  }

protected:
  static void printLine(const std::string_view &line) {
    std::println("{}", line); 
  }

  static void printLine(const std::string_view &line, uint r, uint g, uint b) {
    std::println("{}", rgb(line.data(), r, g, b));
  }

  void writeLine(const std::string_view &message, const std::string_view &severity = "INFO") {
    auto now = std::chrono::system_clock::now();
    auto time = std::format("{:%Y-%m-%d %H:%M:%S}", now);
    auto line = std::format("[{}] {} {}", time, severity, trim(message));

    file << line << "\n";
  }
};
