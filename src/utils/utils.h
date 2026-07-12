#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

std::vector<std::string> glob(fs::path &dir);