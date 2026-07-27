#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

template <typename T>
inline std::string spark(const std::vector<T> &seq) {
  std::vector<std::string> chars = {
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
    const auto x = static_cast<double>(value);
    const auto rescaled = (x - range.min) / (range.max - range.min);
    const auto bin = std::floor(rescaled * (nBins - 1));

    const auto r = 255;
    const auto g = std::floor(255 * rescaled);
    const auto b = std::floor(64 * rescaled);
    result += rgb(chars[bin], r, g, b);
  }

  return result + "]";
}