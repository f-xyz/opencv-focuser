#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <future>
#include <mutex>
#include <print>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

inline std::string joinArgs(int nArgs, const char **args) {
  std::string result = "";

  for (int i = 1; i < nArgs; ++i) {
    result += args[i];
    if (i < nArgs - 1) {
      result += " ";
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

  std::sort(result.begin(), result.end());

  return result;
}

////////////////////////////////////////
// Experiments /////////////////////////
////////////////////////////////////////

template <typename T>
inline void print(const T &x) {
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
    auto ms = std::chrono::milliseconds(delayMs);
    std::this_thread::sleep_for(ms);
    callback();
  }).detach();
}

////////////////////////////////////////

template <typename T>
static void fulfillPromise(std::unique_ptr<std::promise<T>> &promise, T value) {
  if (promise) {
    try {
      promise->set_value(value);
    } catch (const std::future_error &) {
      // Promise was already fulfilled elsewhere
    }
    promise.reset();
  }
}

////////////////////////////////////////

class Throttle {
private:
  int delayMs;
  bool isWaiting = false;
  std::function<void()> callback;
  std::mutex mutex;

public:
  explicit Throttle(const int delayMs, std::function<void()> callback)
    : delayMs(delayMs), callback(std::move(callback)) {}

  void call() {
    if (isWaiting) {
      return;
    }

    isWaiting = true;

    std::thread([this]() {
      std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));

      {
        std::lock_guard<std::mutex> lock(mutex);
        isWaiting = false;
      }

      callback();
    }).detach();
  }
};