//
// Created by Mikiller on 2017/1/16.
//

#ifndef NDKTEST_VIDEOUTILS_H
#define NDKTEST_VIDEOUTILS_H

#define TAG "VIDEO_UTILS"

extern "C"{
#include "PrivateUtils.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>

AVCodecID getVideoCodecId();

AVCodecContext* initVideoCodecContext(int, int, int, int, int);

int openVideoEncoder();

int getVideoStreamId(AVFormatContext*);

int initAvVideoFrame();

int analyzeNV21Data(uint8_t*);

int encodeYUV(jboolean, int needFrame = true);

int writeVideoFrame(AVFormatContext*, pthread_mutex_t*);

void setRotate(int);

void flushVideo(AVFormatContext*, pthread_mutex_t*);

void freeVideoReference();

};


#endif //NDKTEST_VIDEOUTILS_H
