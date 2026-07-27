#include "config.h"
#include "indi/INDIClient.h"
#include "math/SharpnessEstimator.h"
#include "math/Solver.h"
#include "logging/Logger.h"
#include "utils/utils.h"
#include "FocuserApp.h"

namespace fs = std::filesystem;
using namespace std::chrono_literals;

int main(const int nArgs, const char **args) {
  setenv("QT_QPA_PLATFORM", "xcb", 1); // Fixes QT windows on Wayland
  // std::signal(SIGSEGV, onSegfault);
  std::println("{}", rgb("OpenCV Focuser v0.0.1\n", 196, 0, 255));

  if (nArgs < 2) {
    std::println("Usage: ./focuser path/to/image/dir");
    return -1;
  }

  //////////////////////////////////////

  Config config;
  Logger logger(config.logFilePath);
  INDIClient indi(logger);
  SharpnessEstimatorLaplacian estimator;
  Solver solver(config, logger);

  FocuserApp app(config, logger, indi, estimator, solver);
  app.connect();
  app.autoFocus();

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

    auto sharpness = SharpnessEstimatorGaussian().getSharpness(roi);
    auto name = fs::path(file).filename().string();
    logger.info("{} -> sharpness: {}", name, sharpness);
  }

  return 0;
}