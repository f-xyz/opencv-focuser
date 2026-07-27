#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <cstdlib>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <opencv2/imgcodecs.hpp>
#include "../fits/FitsReader.h"
#include "colors.h"

namespace fs = std::filesystem;

////////////////////////////////////////
// Strings /////////////////////////////
////////////////////////////////////////

inline std::string joinArgs(const int nArgs, const char **args) {
  std::string result;

  for (int i = 1; i < nArgs; ++i) {
    result += args[i];
    if (i < nArgs - 1) {
      result += ' ';
    }
  }

  return result;
}

inline std::string_view ltrim(std::string_view str) {
  const auto pos = str.find_first_not_of(" \t\r\n");
  return str.substr(std::min(pos, str.size()));
}

inline std::string_view rtrim(std::string_view str) {
  const auto pos = str.find_last_not_of(" \t\r\n");
  return str.substr(0, pos + 1);
}

inline std::string_view trim(std::string_view str) {
  return ltrim(rtrim(str));
}

inline std::string formatDouble(double x) {
  const auto s = std::to_string(x);
  return x > 0
    ? std::format("{}", rgb(s, 0, 128, 0))
    : std::format("{}", rgb(s, 128, 0, 0));
}

////////////////////////////////////////
// Filesystem //////////////////////////
////////////////////////////////////////

inline std::vector<std::string> readDir(const fs::path &dir) {
  std::vector<std::string> result;

  if (!fs::exists(dir)) {
    return result;
  }

  auto iterator = fs::directory_iterator(dir);
  for (const auto &file : iterator) {
    if (file.is_regular_file()) {
      result.push_back(file.path());
    }
  }

  std::ranges::sort(result);

  return result;
}

inline cv::Mat readImage(const std::string &file) {
  const auto ext = fs::path(file).extension().string();

  if (ext == ".fit" || ext == ".fits") {
    FitsReader fits;
    cv::Mat image = fits.read(file);
    return image;
  }

  return cv::imread(file);
}

////////////////////////////////////////
// For fun /////////////////////////////
////////////////////////////////////////

inline void setTimeout(const std::function<void()>& callback, int delayMs) {
  std::thread([delayMs, callback]() {
    const auto ms = std::chrono::milliseconds(delayMs);
    std::this_thread::sleep_for(ms);
    callback();
  }).detach();
}
