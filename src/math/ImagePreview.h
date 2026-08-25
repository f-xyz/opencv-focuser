#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include <tuple>

struct ImagePreviewOptions {
  // TODO
};

class ImagePreview {
public:

  void preview(const cv::Mat &image, const ImagePreviewOptions &options = {}) {
    auto roi = getROI(image, 2);
    auto norm = normalize(roi);
    auto gray = lightness(norm);
    auto [black, white] = soft_range(gray, 10);
    auto clamped = clamp(gray, black, white);
    auto stretched = clahe(clamped, 10, 8);

    // std::println("Black point: {}", black);
    // std::println("White point: {}", white);

    // show(clamped);
    show(stretched);
  }

private:

  cv::Mat getROI(const cv::Mat &image, int div = 2) {
    return image({
      image.cols / 2 - image.cols / (div * 2),
      image.rows / 2 - image.rows / (div * 2),
      image.cols / div,
      image.rows / div
    });
  }

  cv::Mat normalize(const cv::Mat &image) {
    auto [min, max] = range(image);

    double scale = 255.0 / (max - min);
    double shift = -min * scale;

    cv::Mat result;
    image.convertTo(result, CV_8U, scale, shift);

    return result;
  }

  std::tuple<double, double> range(const cv::Mat &image) {
    double min, max;
    cv::minMaxLoc(image, &min, &max);
    return {min, max};
  }

  cv::Mat lightness(const cv::Mat &image) {
    cv::Mat lab, l;
    cv::cvtColor(image, lab, cv::COLOR_BGR2Lab);
    cv::extractChannel(lab, l, 0);
    return l;
  }

  std::vector<int> histogram(const cv::Mat &image, int histSize = 16) {
    const int channels[] = {0};
    const float range[] = {0, 256};
    const float *ranges[] = {range};

    cv::Mat hist;
    cv::calcHist(&image, 1, channels, cv::noArray(),
      hist, 1, &histSize, ranges);

    // Converts cv::Mat<float> -> std::vector<float>
    return hist;
  }

  std::tuple<int, int> soft_range(const cv::Mat &image, int nTopBins) {
    auto hist = histogram(image, 256);

    std::vector<std::pair<int, int>> pairs;
    for (int i = 0; i < hist.size(); ++i) {
      pairs.push_back({ i, hist[i] });
    }

    std::ranges::sort(pairs,
      std::greater {},
      &std::pair<int, int>::second);

    int blackPoint = 255;
    int whitePoint = 0;

    for (int i = 0; i < nTopBins; ++i) {
      int value = pairs[i].first;
      int count = pairs[i].second;

      if (whitePoint < value) whitePoint = value;
      if (blackPoint > value) blackPoint = value;
    }

    return {blackPoint, whitePoint};
  }

  cv::Mat clamp(const cv::Mat &image, int black, int white) {
    cv::Mat result;
    cv::max(image, black, result);
    cv::min(result, white, result);
    cv::normalize(result, result, 0, 255, cv::NORM_MINMAX);

    return result;
  }

  cv::Mat clahe(const cv::Mat &image, double clipLimit = 10.0, int tileSize = 8) {
    auto clahe = cv::createCLAHE(clipLimit, cv::Size(tileSize, tileSize));

    cv::Mat result;
    clahe->apply(image, result);

    return result;
  }

  void show(const cv::Mat &image, const cv::Size size = cv::Size(640, 480)) {
    cv::Mat preview;
    cv::resize(image, preview, size);
    cv::imshow("Preview", preview);
    cv::waitKey(1);
  }
};