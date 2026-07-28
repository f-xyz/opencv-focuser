#include "Solver.h"
#include "../utils/colors.h"
#include <ranges>

double Solver::addPoint(int position, double sharpness) {
  measurements[position].push_back(sharpness);

  const auto delta = lastSharpness ? sharpness - lastSharpness : 0;
  lastSharpness = sharpness;

  return delta;
}

Solution Solver::findBestPosition() {
  auto table = getAveragedMeasurements();
  auto best = findBestPoint(table);
  auto resultType = findResultType(table, best);
  printReport(table, best);
  printSpark(table);

  auto [a, b, c] = findParabolaCoeffs(table);

  if (a > 0) { // It must be upside down, not like U
    logger.error("Parabola fitting failed.");
    return {
      resultType,
      Point {
        .index = 0,
        .position = 0,
        .sharpness = 0,
        .count = 0
      }
    };
  }

  int x = static_cast<int>(std::round(-b / (2 * a))); // The best theoretical position
  double y = a * x * x + b * x + c; // The best theoretical sharpness

  return {
    resultType,
    Point {
      .index = 0,
      .position = x,
      .sharpness = y,
      .count = 1
    }
  };
}

std::vector<Point> Solver::getAveragedMeasurements() {
    std::vector<Point> results;

    for (auto &pair : measurements) {
      const auto index = results.size();
      const auto position = pair.first;
      const auto sharpness = getAverage(pair.second);
      const auto count = pair.second.size();

      results.push_back({
        index,
        position,
        sharpness,
        count
      });
    }

    return results;
}

Point Solver::findBestPoint(const std::vector<Point> &table) {
  Point best{};

  for (auto &point : table) {
    if (point.sharpness > best.sharpness) {
      best.index = point.index;
      best.position = point.position;
      best.sharpness = point.sharpness;
    }
  }

  return best;
}

SolutionType Solver::findResultType(const std::vector<Point> &table,
                                        const Point &best) {
  SolutionType resultType;
  if (best.position == table.front().position) {
    return SolutionType::MoveInward;
  } else if (best.position == table.back().position) {
    return SolutionType::MoveOutward;
  } else {
    return SolutionType::MoveAround;
  }
}

Coeffs Solver::findParabolaCoeffs(const std::vector<Point> &table) {
  int n = table.size();

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

  auto a = coeffs.at<double>(0);
  auto b = coeffs.at<double>(1);
  auto c = coeffs.at<double>(2);

  logger.info("Parabola coeffs.:");
  logger.info("  a: {}", a);
  logger.info("  b: {}", b);
  logger.info("  c: {}", c);

  return {a, b, c};
}

void Solver::printReport(const std::vector<Point> &table, const Point &best) {
  logger.info("\nFocus position / sharpness:");

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
}

void Solver::printSpark(const std::vector<Point> &table) {
  const auto sharpnesses = table
    | std::views::transform(&Point::sharpness)
    | std::ranges::to<std::vector<double>>();

  logger.info("\n{}\n", spark(sharpnesses));
}