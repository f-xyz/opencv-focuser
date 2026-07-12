#include <fitsio.h>
#include <print>
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
#include "FitsReader.h"

DataType getDataType(int bitsPerPixel) {
  switch (bitsPerPixel) {
    case BYTE_IMG:    return {CV_8UC1, TBYTE};
    case SHORT_IMG:   return {CV_16SC1, TSHORT};
    case USHORT_IMG:  return {CV_16UC1, TUSHORT};
    case LONG_IMG:    return {CV_32SC1, TLONG};
    case FLOAT_IMG:   return {CV_32FC1, TFLOAT};
    default:          return {CV_8UC1, TBYTE};
  }
}

DataType getDataType(int bitsPerPixel, bool unsigned16) {
  if (bitsPerPixel == SHORT_IMG && unsigned16) {
    return {CV_16UC1, TUSHORT};
  }

  return getDataType(bitsPerPixel);
}

bool findImageHdu(fitsfile *fptr) {
  int nHdu = 1;
  int hduType = 0;

  while (true) {
    int statusMove = 0;

    if (ffmahd(fptr, nHdu, &hduType, &statusMove) != 0) {
      return false;
    }

    if (hduType == IMAGE_HDU) {
      int nDimensions = 0;
      int statusGetImgDim = 0;
      fits_get_img_dim(fptr, &nDimensions, &statusGetImgDim);

      if (statusGetImgDim == 0 && nDimensions >= 2) {
        return true;
      }
    }
    ++nHdu;
  }

  return false;
}

int getBayerCode(const std::string &pattern) {
  if (pattern == "RGGB") return cv::COLOR_BayerRG2BGR;
  if (pattern == "BGGR") return cv::COLOR_BayerBG2BGR;
  if (pattern == "GRBG") return cv::COLOR_BayerGR2BGR;
  if (pattern == "GBRG") return cv::COLOR_BayerGB2BGR;
  return -1;
}

cv::Mat readFits(std::string file) {
  fitsfile *fptr = nullptr;
  int status = 0;

  if (fits_open_file(&fptr, file.c_str(), READONLY, &status)) {
    std::println("Error opening file {}, status {}", file, status);
    return cv::Mat();
  }

  if (!findImageHdu(fptr)) {
    std::println("Error: no 2D image HDU found in {}", file);
    fits_close_file(fptr, &status);
    return cv::Mat();
  }

  int bitsPerPixel = 0;
  int nDimensions = 0;
  long dimensions[2] = {0, 0};
  double scale = 1.0;
  double zero = 0.0;
  int scaleStatus = 0;
  int zeroStatus = 0;
  char bayerPattern[FLEN_VALUE] = {0};
  int bayerStatus = 0;

  fits_get_img_param(fptr, 2, &bitsPerPixel, &nDimensions, dimensions, &status);
  printf("  bitsPerPixel: %i\n", bitsPerPixel);
  printf("  nDimensions: %i\n", nDimensions);
  printf("  dimensions: {%li, %li}\n", dimensions[0], dimensions[1]);
  printf("  fits_get_img_param status: %i\n", status);

  if (status != 0) {
    fits_close_file(fptr, &status);
    return cv::Mat();
  }

  fits_read_key(fptr, TDOUBLE, "BSCALE", &scale, nullptr, &scaleStatus);
  if (scaleStatus == KEY_NO_EXIST) {
    scaleStatus = 0;
    scale = 1.0;
  }

  fits_read_key(fptr, TDOUBLE, "BZERO", &zero, nullptr, &zeroStatus);
  if (zeroStatus == KEY_NO_EXIST) {
    zeroStatus = 0;
    zero = 0.0;
  }

  fits_read_key(fptr, TSTRING, "BAYERPAT", bayerPattern, nullptr, &bayerStatus);
  if (bayerStatus == KEY_NO_EXIST) {
    bayerStatus = 0;
    bayerPattern[0] = '\0';
  }

  printf("  BSCALE: %f\n", scale);
  printf("  BZERO: %f\n", zero);
  if (bayerPattern[0] != '\0') {
    printf("  BAYERPAT: %s\n", bayerPattern);
  }

  bool unsigned16 = bitsPerPixel == SHORT_IMG && scale == 1.0 && zero == 32768.0;
  auto [cvType, fptrDataType] = getDataType(bitsPerPixel, unsigned16);

  auto [width, height] = dimensions;
  long nPixels = width * height;
  cv::Mat raw(height, width, cvType);

  long firstPixel[] = {1, 1};
  fits_read_pix(fptr, fptrDataType, firstPixel, nPixels, nullptr,
    raw.data, nullptr, &status);
  printf("  fits_read_pix status: %i\n", status);

  if (status != 0) {
    fits_close_file(fptr, &status);
    return cv::Mat();
  }

  fits_close_file(fptr, &status);
  printf("  fits_close_file status: %i\n", status);

  if (bayerPattern[0] != '\0') {
    const int bayerCode = getBayerCode(bayerPattern);
    if (bayerCode >= 0) {
      cv::Mat bgr;
      cv::cvtColor(raw, bgr, bayerCode);
      return bgr;
    }

    std::println("Warning: unsupported BAYERPAT {} in {}, returning raw image", bayerPattern, file);
  }

  return raw;
}

cv::Mat FitsReader::readFits(std::string file) {
  return ::readFits(file);
}
