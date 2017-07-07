//
// Created by Mikiller on 2017/6/30.
//

#ifndef NDKTEST_TEST_H
#define NDKTEST_TEST_H

#endif //NDKTEST_TEST_H
extern "C"{
#include <libyuv.h>
#include <string.h>

enum VideoType {
       kUnknown,
       kI420,
       kIYUV,
       kRGB24,
       kABGR,
       kARGB,
       kARGB4444,
       kRGB565,
       kARGB1555,
       kYUY2,
       kYV12,
       kUYVY,
       kMJPG,
       kNV21,
       kNV12,
       kBGRA,
     };
};