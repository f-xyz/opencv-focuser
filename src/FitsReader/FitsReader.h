#pragma once

#include <fitsio.h>
#include <string>
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

struct DataType {
  int cvType;
  int fitsType;
};

struct FitsKeywords {
  double scale = 1.0;
  double zero = 0.0;
  char bayer[FLEN_VALUE] = {0};
};

DataType getDataType(int bitsPerPixel);
bool findImageHdu(fitsfile *fptr);
int getBayerCode(const std::string &pattern);
cv::Mat readFits(std::string file);

////////////////////////////////////////

class FitsReader {
  public:
  cv::Mat readFits(std::string file);
};