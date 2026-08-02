#pragma once

#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/photo.hpp>

struct ImageStretcherOptions {
  enum Type { CLAHE, Asinh };

  Type type = Type::CLAHE;
  double claheClipLimit = 10;
  int claheTileSize = 8;
  float asinhFactor = 100;
  int denoiseH = 5;
};

class ImageStretcher {
  cv::Mat image;

public:
  ImageStretcher(const cv::Mat &image) : image(std::move(image)) {}

  cv::Mat stretch(const ImageStretcherOptions &options = {}) {
    cv::cvtColor(image, image, cv::COLOR_BGR2Lab);

    std::vector<cv::Mat> channels;
    cv::split(image, channels);

    if (options.type == ImageStretcherOptions::CLAHE) {
      channels[0] = stretchClahe( channels[0],
        options.claheClipLimit,
        options.claheTileSize);
    } else {
      channels[0] = stretchAsinh(channels[0], options.asinhFactor);
      channels[0].convertTo(channels[0], CV_8U);
    }

    if (options.denoiseH > 0) {
      cv::fastNlMeansDenoising(channels[0], channels[0],
        options.denoiseH, 7, 21);
    }

    cv::merge(channels, image);
    cv::cvtColor(image, image, cv::COLOR_Lab2BGR);

    return image;
  }

private:
  static cv::Mat stretchClahe(const cv::Mat &image, double clipLimit = 50.0, int tileSize = 8) {
    cv::Mat result;
    auto clahe = cv::createCLAHE(clipLimit, cv::Size(tileSize, tileSize));
    clahe->apply(image, result);
    return result;
  }

  static cv::Mat stretchAsinh(const cv::Mat &image, float factor = 100) {
    cv::Mat result;
    image.convertTo(result, CV_32F);

    // Subtract the minimal value
    double min, max;
    cv::minMaxLoc(result, &min, &max);
    result -= min;

    // Stretch
    result.forEach<float>([factor](float &pixel, const int *position) {
      pixel = std::asinh(factor * pixel) / std::asinh(factor);
    });

    cv::normalize(result, result, 0, 255, cv::NORM_MINMAX, CV_32S);

    return result;
  }
};