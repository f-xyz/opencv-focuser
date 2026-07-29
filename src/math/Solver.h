#pragma once

#include "../logging/Logger.h"
#include "../utils/spark.h"
#include <cmath>
#include <functional>
#include <map>
#include <opencv2/core/base.hpp>
#include <string>
#include <vector>

struct Point {
  std::size_t index = 0; // Index of a measurement
  int position = 0; // Focuser position (X)
  double sharpness = 0; // Sharpness (Y)
  std::size_t count = 0; // Number of measurements per position
};

struct Coeffs {
  double a = 0; // a*X^2 +
  double b = 0; // b*X +
  double c = 0; // c
};

enum SolutionType {
  Inward,
  Around,
  Outward,
};

struct Solution {
  SolutionType type {};
  Point bestPoint {};
  Point idealPoint {};
};

class Solver {
  Logger &logger;
  std::map<int, std::vector<double>> measurements;
  double lastSharpness = 0;

public:
  explicit Solver(Logger &logger) : logger(logger) {}

  double addPoint(int position, double sharpness);
  Solution findBestPosition();

  void clear() {
    measurements.clear();
    lastSharpness = 0;
  }
  
private:
  std::vector<Point> getAveragedMeasurements();
  Point findBestPoint(const std::vector<Point> &table);
  SolutionType findResultType(const std::vector<Point> &table, const Point &best);
  Coeffs findParabolaCoeffs(const std::vector<Point> &table);

  void printReport(const std::vector<Point> &table, const Point &best);
  void printSpark(const std::vector<Point> &table);
  void printCoeffs(const Coeffs &coefs);

  double getAverage(const std::vector<double> &values) {
    const auto sum = std::ranges::fold_left( values, 0.0, std::plus {});
    return sum / values.size();
  }

  std::string formatPosition(const int position) {
    return position >= 0
      ? "+" + std::to_string(std::abs(position))
      : "-" + std::to_string(std::abs(position));
  }
};