#pragma once

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <print>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

std::string joinArgs(int nArgs, const char **args);
std::vector<std::string> readDir(const fs::path &dir);

////////////////////////////////////////
// Fun area ////////////////////////////
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