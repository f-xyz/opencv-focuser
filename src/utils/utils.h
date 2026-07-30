#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
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

inline std::string joinArgs(int argc, const char **argv) {
  std::string result;

  for (int i = 1; i < argc; ++i) {
    if (!result.empty()) {
      result += ' ';
    }
    result += argv[i];
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

inline std::string formatNumber(double x) {
  const auto str = std::to_string(x);

  if (x > 0) {
    return std::format("{}", rgb(str, 0, 128, 0));
  } else if (x < 0) {
    return std::format("{}", rgb(str, 128, 0, 0));
  } else {
    return std::format("{}", str);
  }
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
  auto ext = fs::path(file).extension().string();
  return ext == ".fit" || ext == ".fits"
    ? FitsReader().read(file)
    : cv::imread(file);
}

////////////////////////////////////////
// Fun /////////////////////////////////
////////////////////////////////////////

inline void setTimeout(const std::function<void()>& callback, std::chrono::milliseconds delay) {
  std::thread([delay, callback]() {
    std::this_thread::sleep_for(delay);
    callback();
  }).detach();
}
