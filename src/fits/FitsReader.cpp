#include <fitsio.h>
#include <longnam.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include <print>
#include <string>
#include <vector>
#include "FitsReader.h"

cv::Mat FitsReader::read(const std::string &file) {
  const auto fptr = openFile(file);

  if (!findFirstImageHdu(fptr)) {
    return {};
  }

  const auto params = getImageParams(fptr);
  const auto image = readImage(fptr, params);
  closeFile(fptr);
  
  auto result = demosaic(image, params.bayer);
  return result;
}

cv::Mat FitsReader::read(void *data, size_t size) {
  fitsfile *fptr = nullptr;
  int status = 0;

  if (fits_open_memfile(&fptr, "", READONLY, &data, &size, 0, nullptr, &status)) {
    return {};
  }

  if (!findFirstImageHdu(fptr)) {
    return {};
  }

  const auto params = getImageParams(fptr);
  const auto image = readImage(fptr, params);
  closeFile(fptr);
  
  auto result = demosaic(image, params.bayer);
  return result;
}

fitsfile *FitsReader::openFile(const std::string &file) {
  fitsfile *fptr = nullptr;
  int status = 0;

  if (fits_open_file(&fptr, file.c_str(), READONLY, &status)) {
    std::println("Error opening file {}, status {}.", file, status);
    return nullptr;
  }

  return fptr;
}

bool FitsReader::findFirstImageHdu(fitsfile *fptr) {
  int nHdu = 1;
  int hduType = 0;

  while (true) {
    int moveStatus = 0;
    if (fits_movabs_hdu(fptr, nHdu, &hduType, &moveStatus) != 0) {
      closeFile(fptr);
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

  std::println("No 2D image HDU found.");
  closeFile(fptr);

  return false;
}

ImageParams FitsReader::getImageParams(fitsfile *fptr) {
  int status = 0;
  ImageParams result;

  // Dimensions
  fits_get_img_param(fptr, 2, nullptr, &result.nDimensions, result.dimensions, &status);
  if (status != 0) {
    std::println("fits_get_img_param failed {}.", status);
    return {};
  }

  // Bits per pixel
  fits_get_img_equivtype(fptr, &result.bitsPerPixel, &status);
  if (status != 0) {
    std::println("fits_get_img_equivtype failed {}.", status);
    return {};
  }

  // Bayer array pattern
  fits_read_key(fptr, TSTRING, "BAYERPAT", &result.bayer, nullptr, &status);
  if (status != 0) {
    status = 0;
    result.bayer[0] = '\0';
  }

  return result;
}

cv::Mat FitsReader::readImage(fitsfile *fptr, const ImageParams &params) {
  auto [width, height] = params.dimensions;
  auto [cvType, fitsType] = getDataTypes(params);
  cv::Mat image(height, width, cvType);

  int status = 0;
  std::vector<long> firstPixel(params.nDimensions, 1); // FITS starts counting from 1
  fits_read_pix(fptr, fitsType, firstPixel.data(),
    width * height, nullptr, image.data, nullptr,
    &status);

  if (status != 0) {
    return {};
  }

  return image;
}

DataType FitsReader::getDataTypes(const ImageParams &params) {
  switch (params.bitsPerPixel) {
    case BYTE_IMG:    return {CV_8UC1, TBYTE};
    case SHORT_IMG:   return {CV_16SC1, TSHORT};
    case USHORT_IMG:  return {CV_16UC1, TUSHORT};
    case LONG_IMG:    return {CV_32SC1, TLONG};
    case FLOAT_IMG:   return {CV_32FC1, TFLOAT};
    case DOUBLE_IMG:  return {CV_64FC1, TDOUBLE};
    default:          return {CV_8UC1, TBYTE};
  }
}

cv::Mat FitsReader::demosaic(const cv::Mat &image, const char *bayer) {
  if (bayer[0] != '\0') {
    const int bayerCode = getBayerCode(bayer);
    if (bayerCode >= 0) {
      cv::Mat bgr;
      cv::cvtColor(image, bgr, bayerCode);
      return bgr;
    } else {
      std::println("Unsupported BAYERPAT {}, returning raw image.", bayer);
    }
  }

  return image;
}

int FitsReader::getBayerCode(const std::string &pattern) {
  if (pattern == "RGGB") return cv::COLOR_BayerRG2BGR_EA;
  if (pattern == "BGGR") return cv::COLOR_BayerBG2BGR_EA;
  if (pattern == "GRBG") return cv::COLOR_BayerGR2BGR_EA;
  if (pattern == "GBRG") return cv::COLOR_BayerGB2BGR_EA;
  return -1;
}

int FitsReader::closeFile(fitsfile *fptr) {
  int status = 0;
  fits_close_file(fptr, &status);
  return status;
}