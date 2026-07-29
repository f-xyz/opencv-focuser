#include "Config.h"
#include "indi/INDIClient.h"
#include "math/SharpnessEstimator.h"
#include "math/Solver.h"
#include "logging/Logger.h"
#include "utils/colors.h"
#include "FocuserApp.h"
#include <print>
#include <stacktrace>
#include <csignal>

void onSegfault(int signal) {
  std::println("Segmentation fault:");
  std::println("{}", std::stacktrace::current());
  std::signal(signal, SIG_DFL);
  std::raise(signal);
}

int main(const int argc, const char **argv) {
  setenv("QT_QPA_PLATFORM", "xcb", 1); // Fixes QT windows on Wayland
  std::signal(SIGSEGV, onSegfault);
  std::println("{}", rgb("OpenCV Focuser v0.0.1\n", 196, 0, 255));

  //////////////////////////////////////

  Config config;
  if (!config.parse(argc, argv)) {
    return 0;
  }

  Logger logger(config.logFilePath);
  INDIClient indi(logger);
  SharpnessEstimatorGaussian estimator;
  Solver solver(logger);

  FocuserApp app(config, logger, indi, estimator, solver);
  if (app.connect()) {
    app.autoFocus();
  }

  return 0;
}