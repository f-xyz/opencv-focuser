#include <cmath>
#include <fitsio.h>
#include <print>
#include <cstring>
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

DataType getDataType(int bitsPerPixel, bool isUnsigned16) {
  if (bitsPerPixel == SHORT_IMG && isUnsigned16) {
    return {CV_16UC1, TUSHORT};
  }

  switch (bitsPerPixel) {
    case BYTE_IMG:    return {CV_8UC1, TBYTE};
    case SHORT_IMG:   return {CV_16SC1, TSHORT};
    case USHORT_IMG:  return {CV_16UC1, TUSHORT};
    case LONG_IMG:    return {CV_32SC1, TLONG};
    case FLOAT_IMG:   return {CV_32FC1, TFLOAT};
    default:          return {CV_8UC1, TBYTE};
  }
}

bool findImageHdu(fitsfile *fptr) {
  int nHdu = 1;
  int hduType = 0;

  while (true) {
    int moveStatus = 0;

    if (ffmahd(fptr, nHdu, &hduType, &moveStatus) != 0) {
      return false;
    }

    if (hduType == IMAGE_HDU) {
      int nDimensions = 0;
      int getImgDimStatus = 0;
      fits_get_img_dim(fptr, &nDimensions, &getImgDimStatus);

      if (getImgDimStatus == 0 && nDimensions >= 2) {
        return true; // Found the image!
      }
    }
    ++nHdu;
  }

  return false;
}

int getBayerCode(const std::string &pattern) {
  if (pattern == "RGGB") return cv::COLOR_BayerRG2BGR_EA;
  if (pattern == "BGGR") return cv::COLOR_BayerBG2BGR_EA;
  if (pattern == "GRBG") return cv::COLOR_BayerGR2BGR_EA;
  if (pattern == "GBRG") return cv::COLOR_BayerGB2BGR_EA;
  return -1;
}

FitsKeywords getKeywords(fitsfile *fptr) {
  FitsKeywords keywords;
  int status = 0;

  fits_read_key(fptr, TDOUBLE, "BSCALE", &keywords.scale, nullptr, &status);
  if (status == KEY_NO_EXIST) {
    keywords.scale = 1.0;
    status = 0;
  }

  fits_read_key(fptr, TDOUBLE, "BZERO", &keywords.zero, nullptr, &status);
  if (status == KEY_NO_EXIST) {
    keywords.zero = 0.0;
    status = 0;
  }

  fits_read_key(fptr, TSTRING, "BAYERPAT", &keywords.bayer, nullptr, &status);
  if (status == KEY_NO_EXIST) {
    keywords.bayer[0] = '\0';
    status = 0;
  }

  return keywords;
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

  fits_get_img_param(fptr, 2, &bitsPerPixel, &nDimensions, dimensions, &status);
  std::println("  bitsPerPixel: {}", bitsPerPixel);
  std::println("  nDimensions: {}", nDimensions);
  std::println("  dimensions: {} x {}", dimensions[0], dimensions[1]);
  std::println("  fits_get_img_param status: {}", status);

  if (status != 0) {
    fits_close_file(fptr, &status);
    return cv::Mat();
  }

  FitsKeywords keywords = getKeywords(fptr);
  std::println("  BSCALE: {}", keywords.scale);
  std::println("  BZERO: {}", keywords.zero);
  std::println("  BAYERPAT: {}", keywords.bayer);

  bool isUnsigned16 = bitsPerPixel == SHORT_IMG &&
                      keywords.scale == 1.0 &&
                      keywords.zero == 32768.0;
  auto [cvType, fptrDataType] = getDataType(bitsPerPixel, isUnsigned16);

  auto [width, height] = dimensions;
  long nPixels = width * height;
  cv::Mat raw(height, width, cvType);

  long firstPixel[] = {1, 1};
  fits_read_pix(fptr, fptrDataType, firstPixel, nPixels, nullptr,
    raw.data, nullptr, &status);
  std::println("  fits_read_pix status: {}", status);

  if (status != 0) {
    fits_close_file(fptr, &status);
    return cv::Mat();
  }

  fits_close_file(fptr, &status);
  std::println("  fits_close_file status: {}", status);

  if (keywords.bayer[0] != '\0') {
    const int bayerCode = getBayerCode(keywords.bayer);
    if (bayerCode >= 0) {
      cv::Mat bgr;
      cv::cvtColor(raw, bgr, bayerCode);
      return bgr;
    }

    std::println("Warning: unsupported BAYERPAT {} in {}, returning raw image",
       keywords.bayer, file);
  }

  return raw;
}

cv::Mat FitsReader::readFits(std::string file) {
  return ::readFits(file);
}
