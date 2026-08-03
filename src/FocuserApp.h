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

public:

  enum Type {
    ByEar,
    Linear
  };

  struct ImageResult {
    cv::Mat image {};
    double sharpness = 0;
    double delta = 0;
  };

  explicit FocuserApp(Config &config, Logger &logger, INDIClient &indi,
    SharpnessEstimator &estimator, Solver &solver) :
    config(config),
    logger(logger),
    indi(indi),
    estimator(estimator),
    solver(solver) {}

  ~FocuserApp();

  bool connect();
  bool autoFocus(Type type = Type::ByEar, bool startOutward = true);

private:
  bool checkSolution(const Solution &solution);
  bool validateSolution(const Solution &solution);

  void gatherDataByEar(bool startOutward = true);
  void gatherDataLinearly();

  void reportCameras();
  void reportFocusers();

  ImageResult image(double exposure);
  int focusRel(bool isOutward, unsigned int steps);
  int focusAbs(int position);
  void focusCheckLimits(bool isOutward, unsigned int steps);

  static void preview(const cv::Mat &image);
  static cv::Mat getROI(const cv::Mat &image);
  static std::string typeToString(Type type);
};