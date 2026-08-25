#pragma once

#include "colors.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

template <typename T>
inline std::string spark(const std::vector<T> &seq) {
  static const std::vector<std::string> chars = {
    "▁",
    "▂",
    "▃",
    "▄",
    "▅",
    "▆",
    "▇",
    "█"
  };

  if (seq.empty()) {
    return "[]";
  }

  const auto range = std::ranges::minmax(seq);
  const auto nBins = chars.size();

  std::string result = "[";

  for (const auto &value : seq) {
    double x = static_cast<double>(value);
    double rescaled = (x - range.min) / (range.max - range.min);
    double bin = std::floor(rescaled * (nBins - 1));

    int r = 255;
    int g = std::floor(255 * rescaled);
    int b = std::floor(64 * rescaled);
    result += rgb(chars[bin], r, g, b);
  }

  return result + "]";
}