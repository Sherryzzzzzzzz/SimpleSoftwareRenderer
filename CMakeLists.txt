cmake_minimum_required(VERSION 3.10)
project(SoftRenderer)

set(CMAKE_CXX_STANDARD 17)

set(OpenCV_DIR "D:/OpenCV/opencv/build") 
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

include_directories("D:/Visual Stdio/lib/eigen-5.0.1") 

add_executable(SoftRenderer main.cpp MathUtils.cpp MathUtils.h Renderer.cpp Renderer.h LoadModel.cpp LoadModel.h)

target_link_libraries(SoftRenderer ${OpenCV_LIBS})