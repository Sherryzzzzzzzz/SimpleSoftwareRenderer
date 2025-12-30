cmake_minimum_required(VERSION 3.10)
project(SoftRenderer)

set(CMAKE_CXX_STANDARD 17)
find_package(OpenCV REQUIRED)
include_directories("${CMAKE_SOURCE_DIR}/libs/eigen-5.0.1")
add_executable(SoftRenderer main.cpp MathUtils.cpp MathUtils.h Renderer.cpp Renderer.h LoadModel.cpp LoadModel.h "Skybox.h")

target_link_libraries(SoftRenderer ${OpenCV_LIBS})