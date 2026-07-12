#include <cassert>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <fmt/color.h>
#include <fmt/core.h>
#include <format>
#include <longnam.h>
#include <opencv2/core.hpp>
#include <opencv2/core/base.hpp>
#include <opencv2/core/hal/interface.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/matx.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv4/opencv2/imgcodecs.hpp>
#include <string>
#include <unistd.h>
#include <vector>
#include <print>
#include <fitsio.h>
#include "utils/utils.h"
#include "FitsReader/FitsReader.h"

namespace fs = std::filesystem;

std::expected<cv::Mat, std::string> readFile(const std::string &file) {
  auto ext = fs::path(file).extension().string();

  if (ext.ends_with(".fit")) {
    return readFits(file);
  } else {
    auto mat = cv::imread(file);
    if (!mat.empty()) {
      return mat;
    } else {
      return std::unexpected("Failed to decode image or file missing: " + file);
    }
  }
}

double getSharpness(cv::Mat image) {
  double sigmaNarrow = 1;
  double sigmaWide = 10;

  cv::Mat narrow, wide, dog;
  cv::Size kernel = cv::Size(0, 0);
  cv::GaussianBlur(image, narrow, kernel, sigmaNarrow, sigmaNarrow);
  cv::GaussianBlur(image, wide, kernel, sigmaWide, sigmaWide);
  cv::subtract(narrow, wide, dog);

  cv::Scalar mean, stdDev;
  cv::meanStdDev(dog, mean, stdDev);

  return stdDev[0];
}

int main(int nArgs, char **args) {
  setenv("QT_QPA_PLATFORM", "xcb", 1); // Fixes QT windows on Wayland
  fmt::print(fg(fmt::color::violet), "OpenCV Focuser Client\n");

  // Arguments: move to App
  if (nArgs < 2) {
    std::println("Usage: focuser path/to/image/dir");
    return -1;
  }

  // Glob
  fs::path dir = args[1];
  std::vector files = glob(dir);
  for (const auto &file : files) {
    fs::path path = fs::path(file);
    const auto name = path.filename().string();
    const auto ext = path.extension().string();

    auto result = readFile(file);
    if (result) {
      auto image = *result;

      // cv::Mat preview;
      // cv::resize(image, preview, cv::Size(640, 480));
      // cv::imshow("Image", preview);
      // cv::waitKey(0);

      const auto sharpness = getSharpness(image);
      std::println("{} -> sharpness: {}", name, sharpness);
    } else {
      std::println(stderr, "{}", result.error());
    }
  }

  return 0;
}
