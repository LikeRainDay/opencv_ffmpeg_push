cmake_minimum_required(VERSION 3.16)
project(opencv_ffmpeg_push)
set(CMAKE_CXX_STANDARD 14)

set(FFMPEG_DIR /usr/local/Cellar/ffmpeg/4.2.3_1/)
set(OPENCV_DIR /usr/local/opt/opencv@4/)

# ffmpeg 库
include_directories(${FFMPEG_DIR}/include/)
link_directories(${FFMPEG_DIR}/lib/)

include_directories(${OPENCV_DIR}/include/opencv4/)
link_directories(${OPENCV_DIR}/lib/)

add_executable(opencv_ffmpeg_push main.cpp)

target_link_libraries(
        opencv_ffmpeg_push
        avcodec.58
        avdevice.58
        avfilter.7
        avformat.58
        avutil.56
        postproc.55
        swresample.3
        swscale.5
        opencv_xphoto
        opencv_xobjdetect
        opencv_ximgproc
        opencv_xfeatures2d
        opencv_viz
        opencv_videostab
        opencv_videoio
        opencv_video
        opencv_tracking
        opencv_text
        opencv_surface_matching
        opencv_superres
        opencv_structured_light
        opencv_stitching
        opencv_stereo
)