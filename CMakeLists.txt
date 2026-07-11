cmake_minimum_required(VERSION 3.15)
project(Focuser VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(fmt REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenCV REQUIRED)

add_executable(focuser focuser.cpp)

target_include_directories(focuser PRIVATE
  ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(focuser PRIVATE
  ${OpenCV_LIBS}
  fmt::fmt
  nlohmann_json::nlohmann_json
)