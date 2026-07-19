#include "utils.h"
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

std::string joinArgs(int nArgs, const char **args) {
  std::string result = "";

  for (int i = 1; i < nArgs; ++i) {
    result += args[i];
    if (i < nArgs - 1) {
      result += " ";
    }
  }

  return result;
}

std::vector<std::string> readDir(const fs::path &dir) {
  std::vector<std::string> result;

  if (!fs::exists(dir)) {
    return result;
  }

  auto iterator = fs::directory_iterator(dir);
  for (const auto &file : iterator) {
    if (file.is_regular_file()) {
      result.push_back(file.path());
    }
  }

  std::sort(result.begin(), result.end());

  return result;
}