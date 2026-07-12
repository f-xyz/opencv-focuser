#pragma once

#include <filesystem>
#include <print>
#include <string>
#include <vector>

namespace fs = std::filesystem;

std::string joinArgs(int nArgs, const char **args);
std::vector<std::string> readDir(const fs::path &dir);

template <typename T>
inline void print(const T &x) {
  std::println("{}", x);
}

template <typename T>
class ElementPrinter {
public:
  void operator()(const T &x) const { std::println("{}", x); }
};