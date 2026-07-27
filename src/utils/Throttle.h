#pragma once

#include <functional>
#include <mutex>
#include <thread>

class Throttle final {
  bool isWaiting = false;
  std::chrono::milliseconds delay;
  std::function<void()> callback;
  std::mutex mutex;

public:
  explicit Throttle(
    const std::chrono::milliseconds delay,
    const std::function<void()> &callback) :
     delay(delay),
     callback(callback) {}

  void call() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (isWaiting) {
        return;
      }
      isWaiting = true;
    }

    std::thread([this]() {
      std::this_thread::sleep_for(delay);

      {
        std::lock_guard<std::mutex> lock(mutex);
        isWaiting = false;
      }

      callback();
    }).detach();
  }
};
