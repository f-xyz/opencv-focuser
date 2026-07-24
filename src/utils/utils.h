#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <future>
#include <ranges>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <opencv2/imgcodecs.hpp>
#include "../fits/FitsReader.h"

namespace fs = std::filesystem;

////////////////////////////////////////
// Strings /////////////////////////////
////////////////////////////////////////

inline constexpr std::string joinArgs(const int nArgs, const char **args) {
  std::string result;

  for (int i = 1; i < nArgs; ++i) {
    result += args[i];
    if (i < nArgs - 1) {
      result += ' ';
    }
  }

  return result;
}

inline constexpr std::string_view ltrim(std::string_view str) {
  const auto pos = str.find_first_not_of(" \t\r\n");
  return str.substr(std::min(pos, str.size()));
}

inline constexpr std::string_view rtrim(std::string_view str) {
  const auto pos = str.find_last_not_of(" \t\r\n");
  return str.substr(0, pos + 1);
}

inline constexpr std::string_view trim(std::string_view str) {
  return ltrim(rtrim(str));
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

inline cv::Mat readFile(const std::string &file) {
  const auto ext = fs::path(file).extension().string();

  if (ext == ".fit" || ext == ".fits") {
    FitsReader fits;
    cv::Mat image = fits.read(file);
    return image;
  }

  return cv::imread(file);
}

////////////////////////////////////////
// Experiments /////////////////////////
////////////////////////////////////////

template <typename T>
void print(const T &x) {
  std::println("{}", x);
}

template <typename T>
class ElementPrinter {
public:
  void operator()(const T &x) const { std::println("{}", x); }
};

template <typename T>
void spark(const std::vector<T> &seq) {
  std::vector<char32_t> ticks = {
    U'▁',
    U'▂',
    U'▃',
    U'▄',
    U'▅',
    U'▆',
    U'▇',
    U'█'
  };

  const auto n = ticks.size();

  // Normalize the sequence
  const auto sum = 
  std::ranges::fold_left(seq, 0.0, [](auto acc, const auto& item) {
    return acc + static_cast<double>(item);
  });

  const auto min = std::ranges::min(seq);
  std::println("sum: {}", sum);
  std::println("min: {}", min);

  auto normalized = seq | std::views::transform([sum](auto x) {
    return static_cast<double>(x) / sum;
  });

  for (const auto x : seq) {
    auto bin = floor((static_cast<double>(x) - min) / n);
    std::println("{} / bin #{}", x, 0);
  }
}

////////////////////////////////////////

inline void setTimeout(const std::function<void()>& callback, int delayMs) {
  std::thread([delayMs, callback]() {
    const auto ms = std::chrono::milliseconds(delayMs);
    std::this_thread::sleep_for(ms);
    callback();
  }).detach();
}

template <typename T>
void fulfillPromise(std::unique_ptr<std::promise<T>> &promise, T value) {
  if (promise) {
    try {
      promise->set_value(value);
    } catch (const std::future_error &) {
      // Promise was already fulfilled elsewhere
    }
    promise.reset();
  }
}