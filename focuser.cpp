#include <cstdio>
#include <fmt/color.h>
#include <fmt/core.h>
#include <memory>
#include <opencv2/core.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/matx.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv4/opencv2/imgcodecs.hpp>
#include <unistd.h>
#include <vector>

using namespace fmt;

class App {
public:
  App() { printf("App()\n"); }
  virtual ~App() { printf("~App()\n"); }
};

int main(int nArgs, char **args) {
  fmt::print(fg(fmt::color::violet), "OpencV Focuser\n");
  setenv("QT_QPA_PLATFORM", "xcb", 1);

  // Arguments
  for (int i = 0; i < nArgs; ++i) {
    const char *arg = args[i];
    printf("Arg #%i: %s\n", i, arg);
  }

  //////////////////////////////////////

  auto path = "images/random/sky1.jpg";
  auto input = cv::imread(path, cv::IMREAD_GRAYSCALE);

  auto size = cv::Size(640, 480);
  cv::Mat image, blurred, bw;
  cv::resize(input, image, size);
  cv::threshold(image, bw, 127, 255, cv::THRESH_BINARY);

  // cv::adaptiveThreshold(image, bw, 255,
  //   cv::ADAPTIVE_THRESH_GAUSSIAN_C,
  //   cv::THRESH_BINARY_INV, 41, 20);

  //////////////////////////////////////

  std::vector<std::vector<cv::Point>> contours;
  std::vector<cv::Vec4i> hierarchy;
  cv::findContours(bw, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

  cv::Mat result = image.clone();
  cv::Scalar color = cv::Scalar(0, 255, 0);
  cv::drawContours(result, contours, -1, color, 2, cv::LINE_AA);

  //////////////////////////////////////

  cv::imshow("Image", result);
  cv::imshow("Original", image);
  cv::waitKey(0);
  cv::destroyAllWindows();

  auto app = std::make_unique<App>();

  return 0;
}
