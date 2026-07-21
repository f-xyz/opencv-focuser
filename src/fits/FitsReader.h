#pragma once

#include <fitsio.h>
#include <opencv2/core/mat.hpp>
#include <string>

struct DataType {
  int cvType;
  int fitsType;
};

struct ImageParams {
  int bitsPerPixel = 0;
  int nDimensions = 0;
  long dimensions[2] = {0, 0};
  char bayer[FLEN_VALUE] = {0};
};

class FitsReader {
  fitsfile* openFile(const std::string &file);
  bool findFirstImageHdu(fitsfile *fptr);
  ImageParams getImageParams(fitsfile *fptr);
  cv::Mat readImage(fitsfile *fptr, const ImageParams &params);
  DataType getDataTypes(const ImageParams &params);
  cv::Mat demosaic(const cv::Mat &image, const char *bayer);
  int getBayerCode(const std::string &pattern);
  int closeFile(fitsfile *fptr);

public:
  explicit FitsReader() = default;
  cv::Mat read(const std::string& file);
  cv::Mat read(void *data, size_t size);
};