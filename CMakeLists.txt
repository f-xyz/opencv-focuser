cmake_minimum_required(VERSION 3.15)
project(Focuser VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)

find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(CFITSIO REQUIRED cfitsio)

add_executable(focuser 
  src/main.cpp
  src/utils/utils.cpp
  src/FitsReader/FitsReader.cpp
)

target_include_directories(focuser PRIVATE
  ${OpenCV_INCLUDE_DIRS}
  ${CFITSIO_INCLUDE_DIRS}
)

target_link_libraries(focuser PRIVATE
  ${OpenCV_LIBS}
  ${CFITSIO_LIBRARIES}
  fmt::fmt
  nlohmann_json::nlohmann_json
)