#pragma once

#include "../fits/FitsReader.h"
#include "colors.h"
#include <chrono>
#include <filesystem>
#include <functional>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <regex>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

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

template <typename T> requires std::is_arithmetic_v<T>
inline std::string formatNumber(T x) {
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

inline std::string getTmpFileName(const std::string &dir) {
  std::vector<std::string> existing = readDir(dir);
  std::vector<int> indexes;

  std::ranges::transform(existing,
    std::back_inserter(indexes),
    [](const std::string &file) {
      std::smatch matches;
      std::regex regex = std::regex(R"((\d+).*?$)");
      std::regex_search(file, matches, regex);
      int index = matches.size() ? std::stoi(matches[0].str()) : 0;
      return index;
  });

  int maxIndex = indexes.empty() ? 0 : *std::ranges::max_element(indexes);
  int newIndex = maxIndex + 1;

  std::string out = dir.ends_with('/') ? dir : dir + '/';
  return std::format("{}{}", out, newIndex);
}

////////////////////////////////////////
// Images //////////////////////////////
////////////////////////////////////////

inline cv::Mat readImage(const std::string &file) {
  auto ext = fs::path(file).extension().string();
  return ext == ".fit" || ext == ".fits"
    ? FitsReader().read(file)
    : cv::imread(file);
}

inline std::pair<double, double> getImageMinMax(const cv::Mat &image) {
  double min, max;
  cv::minMaxLoc(image, &min, &max);
  return {min, max};
}

inline std::string getImageInfo(const cv::Mat &image) {
  auto type = cv::typeToString(image.type());
  auto minmax = getImageMinMax(image);

  return std::format("{} {}x{} [{}-{}]",
    type, image.cols, image.rows, minmax.first, minmax.second);
}

inline std::vector<float> getImageHistogram(const cv::Mat &image, int histSize = 16) {
  cv::Mat lab;
  cv::cvtColor(image, lab, cv::COLOR_BGR2Lab);

  const auto minmax = getImageMinMax(image);
  const int channels[] = {0};
  const float range[] = {static_cast<float>(minmax.first), static_cast<float>(minmax.second + 1e-6)};
  const float *ranges[] = {range};

  cv::Mat hist;
  cv::calcHist(&lab, 1, channels, cv::noArray(),
    hist, 1, &histSize, ranges);

  // Converts cv::Mat<float> -> std::vector<float>
  return hist;
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
