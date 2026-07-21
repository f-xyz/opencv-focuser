#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <future>
#include <print>
#include <string>
#include <thread>
#include <vector>
#include <opencv2/imgcodecs.hpp>
#include "../fits/FitsReader.h"

namespace fs = std::filesystem;

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