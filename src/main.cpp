#include "config.h"
#include "indi/INDIClient.h"
#include "SharpnessEstimator.h"
#include "logging/Logger.h"
#include "utils/utils.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <future>
#include <opencv2/highgui.hpp>
#include <print>
#include <ranges>
#include <string>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using namespace std::chrono_literals;

class FocuserApp final {
  Config &config;
  Logger &logger;
  INDIClient &indi;
  std::promise<bool> startPromise;

public:
  FocuserApp(Config &config, Logger &logger, INDIClient &indi)
      : config(config), logger(logger), indi(indi) {}

  std::future<bool> connect() {
    auto future = startPromise.get_future();

    auto isConnected = indi.connect(config.indiHost, config.indiPort).get();
    startPromise.set_value(isConnected);

    if (isConnected) {
      logger.info("Ready\n");
    }

    return future;
  }

  void reportCameras() {
    logger.info("Cameras:");
    const auto cameras = indi.getCameras();
    for (int i = 0; i < cameras.size(); ++i) {
      auto marker = i == 0 ? '>' : '*';
      auto &camera = cameras[i];
      logger.info("  {} {}: {}x{}", marker,
        camera.name, camera.width, camera.height);
    }
    logger.info("");
  }

  void reportFocusers() {
    logger.info("Focusers:");
    const auto focusers = indi.getFocusers();
    for (int i = 0; i < focusers.size(); ++i) {
      auto marker = i == 0 ? '>' : '*';
      auto &focuser = focusers[i];
      logger.info("  {} {}: {}", marker, focuser.name, focuser.position);
    }
    logger.info("");
  }
};

class Results {
  Logger &logger;
  std::map<int, std::vector<double>> map;

public:
  Results(Logger &logger) : logger(logger) {}

  void addPoint(int position, double sharpness) {
    map[position].push_back(sharpness);
  }

  void report() {
    logger.info("\nFocus / sharpness:");

    int bestIndex = 0;
    int bestPosition = 0;
    double bestSharpness = 0;

    struct FocusPoint {
      std::size_t index = 0;
      int position = 0;
      double sharpness = 0;
      std::size_t count = 0;
    };

    std::vector<FocusPoint> table;
    for (auto &kv : map) {
      const auto index = table.size();
      const auto position = kv.first;
      const auto sharpness = getAverage(kv.second);
      const auto count = kv.second.size();
      table.push_back({ index, position, sharpness, count });

      if (sharpness > bestSharpness) {
        bestIndex = index;
        bestPosition = position;
        bestSharpness = sharpness;
      }
    }

    for (auto &[index, position, sharpness, count] : table) {
      const auto row = std::format("#{:<2} {:<6}: {:.4f} ({})",
        index,
        formatPosition(position),
        sharpness,
        count);
      logger.info("{}", row);
    }

    ////////////////////////////////////

    const auto sharpnesses = table
       | std::views::transform(&FocusPoint::sharpness)
       | std::ranges::to<std::vector<double>>();
    // printChart(sharpnesses);

    ////////////////////////////////////

    const auto bestPrev = table[bestIndex - 1];
    const auto best = table[bestIndex];
    const auto bestNext = table[bestIndex + 1];

    const auto logPoint = [this](const FocusPoint &x) {
      logger.info("#{} {:<6}: {:.4f}",
        x.index,
        formatPosition(x.position),
        x.sharpness,
        x.count);
    };

    logger.info("\nBest points:");
    logPoint(bestPrev);
    logPoint(best);
    logPoint(bestNext);

    // xBest = x2 + stepSize * (y1 - y3) / (2 * (y1 - 2*y2 + y3));
  }

  double getAverage(const std::vector<double> &values) {
    return getSum(values) / values.size();
  }

  double getSum(const std::vector<double> &values) {
    return std::ranges::fold_left( values, 0.0, [](auto res, const auto &x) {
      return res + x;
    });
  }

  std::string formatPosition(const int position) {
    return position >= 0
      ? "+" + std::to_string(std::abs(position))
      : "-" + std::to_string(std::abs(position));
  }
};

int main(const int nArgs, const char **args) {
  setenv("QT_QPA_PLATFORM", "xcb", 1); // Fixes QT windows on Wayland
  std::signal(SIGSEGV, onSegfault);
  std::println("{}", rgb("OpenCV Focuser v0.0.1\n", 196, 0, 255));

  if (nArgs < 2) {
    std::println("Usage: ./focuser path/to/image/dir");
    return -1;
  }

  printSpark<double>({});
  printSpark<double>({0, 1, 2, 3, 4, 5, 6, 7});
  printSpark<int>({0, 1, 2, 3, 4, 5, 6, 7});
  return 0;

  //////////////////////////////////////

  Config config;
  Logger logger(config.logFilePath);
  INDIClient indi(logger);
  Results results(logger);
  FocuserApp app(config, logger, indi);

  app.connect().get();
  app.reportCameras();
  app.reportFocusers();

  double lastSharpness = 0;
  int lastPosition = 0;
  bool isOutward = true;

  const int nIterations = 10;
  for (int i = 0; i < nIterations; ++i) {
    logger.info("Iteration #{} of {}", i + 1, nIterations);

    auto image = indi.shoot(config.cameraExposure).get();
    auto sharpness = SharpnessEstimator::gaussian(image);
    auto delta = lastSharpness ? sharpness - lastSharpness : 0;
    lastSharpness = sharpness;
    results.addPoint(lastPosition, sharpness);
    logger.info("Sharpness: {}", sharpness);
    logger.info("Delta: {}", delta);

    if (i < nIterations - 1) {
      if (delta < 0) {
        isOutward = !isOutward;
      }

      auto position = indi.focus(isOutward, config.focuserStepSize).get();
      lastPosition = position;
      std::this_thread::sleep_for(1s); // Wait for the vibration to fade
      logger.info("Position: {}\n", position);
    } else {
      const cv::Size size(640, 480);
      cv::Mat preview;
      cv::resize(image, preview, size);
      cv::imshow("Exposure", preview);
      cv::waitKey(0);
      cv::destroyAllWindows();
    }
  }

  results.report();

  return 0;

  //////////////////////////////////////

  const std::string dir = joinArgs(nArgs, args);
  const std::vector files = readDir(dir);

  for (auto &file : files) {
    const cv::Mat image = readFile(file);

    const int w = image.cols;
    const int h = image.rows;
    const auto rect = cv::Rect(w / 4, h / 4, w / 2, h / 2);
    const auto roi = image(rect);

    auto sharpness = SharpnessEstimator::gaussian(roi);
    auto name = fs::path(file).filename().string();
    logger.info("{} -> sharpness: {}", name, sharpness);
  }

  return 0;
}