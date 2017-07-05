set(ffmpegDir "${CMAKE_SOURCE_DIR}/libs/armeabi")
set(ffmpegDir-v7a "${CMAKE_SOURCE_DIR}/libs/armeabi-v7a")

# Sets the minimum version of CMake required to build the native
# library. You should either keep the default value or only pass a
# value of 3.4.0 or lower.

cmake_minimum_required(VERSION 3.4.1)

include_directories(libs/armeabi/include)
#include_directories(libs/armeabi/include/libavcodec)
#include_directories(libs/armeabi/include/libavdevice)
#include_directories(libs/armeabi/include/libavfilter)
#include_directories(libs/armeabi/include/libavformat)
#include_directories(libs/armeabi/include/libavresample)
#include_directories(libs/armeabi/include/libavutil)
#include_directories(libs/armeabi/include/libpostproc)
#include_directories(libs/armeabi/include/libswresample)
#include_directories(libs/armeabi/include/libswscale)
#include_directories(libs/armeabi/include/librtmp)
#include_directories(libs/armeabi/include/libyuv)
include_directories(libs/armeabi/include/librtmp/polarssl)
#include_directories(libs/armeabi/include/fdk-aac)
#include_directories(libs/include)
include_directories(src/main/cpp/)
#include_directories(src/main/cpp/utils)
link_directories(${ffmpegDir})
#link_directories(${ffmpegDir-v7a})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11 -Wno-error=format-security")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static -Wno-error=format-security")

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds it for you.
# Gradle automatically packages shared libraries with your APK.


add_library( # Sets the name of the library.
             ndktest

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             # Associated headers in the same location as their source
             # file are automatically included.
             src/main/cpp/native-lib.cpp
             src/main/cpp/VideoUtils.cpp
             src/main/cpp/AudioUtils.cpp
             src/main/cpp/NDKImpl.cpp)

#add_library( # Sets the name of the library.
#             ndk2
#
#             # Sets the library as a shared library.
#             SHARED
#
#             # Provides a relative path to your source file(s).
#             # Associated headers in the same location as their source
#             # file are automatically included.
#             src/main/cpp/native2.cpp)


# Searches for a specified prebuilt library and stores the path as a
# variable. Because system libraries are included in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in the
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target library.
                       ndktest

                       # Links the target library to the log library
                       # included in the NDK.
                       ${log-lib}
                      avformat-57
                      avcodec-57
                      avdevice-57
                      avutil-55
                      swresample-2
                      postproc-54
                      avfilter-6
                      swscale-4
                      avresample-3
                      fdk-aac
                      x264
                      yuv
#                    rtmp
#                    polarssl
                    )

#target_link_libraries( # Specifies the target library.
#                       ndktest
#
#                       # Links the target library to the log library
#                       # included in the NDK.
#                        avcodec-57)
