#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fmt/color.h>
#include <fmt/core.h>
#include <memory>
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
#include <print>
#include <string>
#include <tuple>
#include <unistd.h>
#include <vector>

using namespace fmt;
namespace fs = std::filesystem;

class App {
public:
  App() { printf("App()\n"); }
  virtual ~App() { printf("~App()\n"); }
};

std::vector<std::string> glob(fs::path &dir) {
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

std::tuple<double, double> getSharpness(std::string file) {
  cv::Mat image = cv::imread(file, cv::IMREAD_GRAYSCALE);

  double sigmaNarrow = 1;
  double sigmaWide = 10;

  cv::Mat narrow, wide, dog;
  cv::Size kernel = cv::Size(0, 0);
  cv::GaussianBlur(image, narrow, kernel, sigmaNarrow, sigmaNarrow);
  cv::GaussianBlur(image, wide, kernel, sigmaWide, sigmaWide);
  cv::subtract(narrow, wide, dog);
  
  // cv::imshow("Image", imageNormalized);
  // cv::waitKey(0);

  cv::Scalar mean, stdDev;
  cv::meanStdDev(dog, mean, stdDev);

  return std::make_tuple(mean[0], stdDev[0]);
}

int main(int nArgs, char **args) {
  fmt::print(fg(fmt::color::violet), "OpencV Focuser\n");
  setenv("QT_QPA_PLATFORM", "xcb", 1); // Fixes QT windows on Wayland

  // Arguments
  for (int i = 0; i < nArgs; ++i) {
    const char *arg = args[i];
    printf("Arg #%i: %s\n", i, arg);
  }

  // Glob
  fs::path dir = args[1];
  std::vector files = glob(dir);
  for (const auto &file : files) {
    const auto name = fs::path(file).filename().string();
    const auto [mean, stdDev] = getSharpness(file);
    std::println("{} stdDev: {}", name, stdDev);
  }

  //////////////////////////////////////

  auto app = std::make_unique<App>();

  return 0;
}
