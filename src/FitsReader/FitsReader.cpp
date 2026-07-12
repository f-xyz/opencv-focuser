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

bool findImageHdu(fitsfile *fptr) {
  int nHdu = 1;
  int hduType = 0;

  while (true) {
    int statusMove = 0;
    
    // Attempt to move to the absolute HDU index
    if (ffmahd(fptr, nHdu, &hduType, &statusMove) != 0) {
        break; // Reached the true end of all extensions
    }

    if (hduType == IMAGE_HDU) {
      int nDimensions = 0;
      int statusGetImgDim = 0;
      fits_get_img_dim(fptr, &nDimensions, &statusGetImgDim);
      
      // A valid pixel array must have at least a 2D grid
      if (statusGetImgDim == 0 && nDimensions >= 2) {
          return true; // Located the true image layer!
      }
    }
    ++nHdu;
  }

  return false;
}

cv::Mat readFits(std::string file) {
  fitsfile *fptr = nullptr;
  int status = 0;

  if (fits_open_file(&fptr, file.data(), READONLY, &status)) {
    std::println("Error opening file {}, status {}", file, status);
    return cv::Mat();
  }

  int bitsPerPixel = 0;
  int nDimensions = 0;
  long dimensions[2] = {0, 0};

  findImageHdu(fptr);

  fits_get_img_param(fptr, 2, &bitsPerPixel, &nDimensions, dimensions, &status);
  printf("  bitsPerPixel: %i\n", bitsPerPixel);
  printf("  nDimensions: %i\n", nDimensions);
  printf("  dimensions: {%li, %li}\n", dimensions[0], dimensions[1]);
  printf("  fits_get_img_param status: %i\n", status);

  if (status != 0) {
    fits_close_file(fptr, &status);
    return cv::Mat();
  }

  auto [cvType, fptrDataType] = getDataType(bitsPerPixel);

  auto [width, height] = dimensions;
  long nPixels = width * height;
  cv::Mat image(height, width, cvType);

  long firstPixel[] = {1, 1};
  fits_read_pix(fptr, fptrDataType, firstPixel, nPixels, nullptr,
    image.data, nullptr, &status);
  printf("  fits_read_pix status: %i\n", status);

  fits_close_file(fptr, &status);
  printf("  fits_close_file status: %i\n", status);

  return image;
}