#pragma once

#include "Config.h"
#include "indi/INDIClient.h"
#include "logging/Logger.h"
#include "math/SharpnessEstimator.h"
#include "math/Solver.h"
#include <opencv2/core.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

using namespace std::chrono_literals;

class FocuserApp final {
  Config &config;
  Logger &logger;
  INDIClient &indi;
  SharpnessEstimator &estimator;
  Solver &solver;

  int focusPosition = 0;
  bool isFocusingOutward = true;

  struct ImageResult {
    cv::Mat image {};
    double sharpness = 0;
    double delta = 0;
  };

public:
  explicit FocuserApp(Config &config, Logger &logger, INDIClient &indi,
    SharpnessEstimator &estimator, Solver &solver) :
    config(config),
    logger(logger),
    indi(indi),
    estimator(estimator),
    solver(solver) {}

  bool connect();
  bool autoFocus();

private:
  bool gatherDataByHeart();
  bool gatherDataLinearly();
  bool checkSolution(const Solution &solution);
  bool validateSolution(const Solution &solution);

  void reportCameras();
  void reportFocusers();

  ImageResult shoot(double exposure);
  int move(const bool isOutward, const unsigned int steps);

  static cv::Mat getROI(const cv::Mat &image);
  static void preview(const cv::Mat &image);
};