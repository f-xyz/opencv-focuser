#pragma once

#include "../config.h"
#include "../logging/Logger.h"
#include <cmath>
#include <functional>
#include <map>
#include <ranges>
#include <string>

class Solver {
  Config &config;
  Logger &logger;
  std::map<int, std::vector<double>> map;
  double lastSharpness = 0;

  struct FocusPoint {
    std::size_t index = 0;
    int position = 0;
    double sharpness = 0;
    std::size_t count = 0;
  };

public:
  Solver(Config &config, Logger &logger) :
    config(config),
    logger(logger) {}

  double addPoint(int position, double sharpness) {
    map[position].push_back(sharpness);

    const auto delta = lastSharpness ? sharpness - lastSharpness : 0;
    lastSharpness = sharpness;
    return delta;
  }

  int findBestPosition() {
    logger.info("\nFocus position / sharpness:");

    ////////////////////////////////////
    // Normalization ///////////////////
    ////////////////////////////////////

    std::vector<FocusPoint> table;
    FocusPoint best {};

    for (auto &kv : map) {
      const auto index = table.size();
      const auto position = kv.first;
      const auto sharpness = getAverage(kv.second);
      const auto count = kv.second.size();
      table.push_back({ index, position, sharpness, count });

      if (sharpness > best.sharpness) {
        best.index = index;
        best.position = position;
        best.sharpness = sharpness;
      }
    }

    ////////////////////////////////////
    // Print report ////////////////////
    ////////////////////////////////////

    for (auto &x : table) {
      const auto row = std::format("#{:<2} {:<6}: {:.4f} ({})",
        x.index,
        formatPosition(x.position),
        x.sharpness,
        x.count);
      logger.info("{}", x.index == best.index
        ? rgb(row, 255, 192, 0)
        : row);
    }

    ////////////////////////////////////
    // Spark! //////////////////////////
    ////////////////////////////////////

    const auto sharpnesses = table
       | std::views::transform(&FocusPoint::sharpness)
       | std::ranges::to<std::vector<double>>();
    logger.info("\n{}\n", spark(sharpnesses));

    ////////////////////////////////////
    // Parabola curve fitting //////////
    ////////////////////////////////////

    int n = static_cast<int>(table.size());
    cv::Mat X(n, 3, CV_64F);
    cv::Mat Y(n, 1, CV_64F);

    for (int i = 0; i < n; ++i) {
      double x = table[i].position;
      double y = table[i].sharpness;

      X.at<double>(i, 0) = x * x;
      X.at<double>(i, 1) = x;
      X.at<double>(i, 2) = 1;
      Y.at<double>(i, 0) = y;
    }

    cv::Mat coeffs;
    cv::solve(X, Y, coeffs, cv::DECOMP_SVD);

    double a = coeffs.at<double>(0);
    double b = coeffs.at<double>(1);
    double c = coeffs.at<double>(2);
    
    logger.info("> a: {}", a);
    logger.info("> b: {}", b);
    logger.info("> c: {}", c);

    if (a > 0) {
      logger.error("Parabola fitting failed.");
      return 0;
    }

    double x = -b / (2 * a);
    double y = a * x * x + b * x + c;
    
    logger.info("> Ideal focus position: {}", std::round(x));

    return x;
  }

  double getAverage(const std::vector<double> &values) {
    return getSum(values) / values.size();
  }

  double getSum(const std::vector<double> &values) {
    return std::ranges::fold_left( values, 0.0, std::plus {});
  }

  std::string formatPosition(const int position) {
    return position >= 0
      ? "+" + std::to_string(std::abs(position))
      : "-" + std::to_string(std::abs(position));
  }
};