//
// Created by Mikiller on 2016/12/8.
//

#ifndef NDKTEST_PRIVATEUTILS_H
#define NDKTEST_PRIVATEUTILS_H

#include "jni.h"
extern "C"{
#include <libavformat/avformat.h>

AVFormatContext *outFormatCxt = NULL;
//AVStream *avStream = NULL;
AVCodecContext *pVideoCodecCxt = NULL;
//use AVCodecParameters instead of AVCodecContext
//AVCodecParameters *pCodecParam = NULL;
AVCodec *avVideoCodec = NULL;
AVPacket avVideoPacket;
AVFrame *avVideoFrame = NULL;
size_t frameBufSize = 0;
uint8_t *frameBuffer = NULL;

AVCodecContext *pAudioCodecCxt = NULL;
AVCodec *avAudioCodec = NULL;
AVPacket avAudioPacket;
AVFrame *avAudioFrame = NULL;
size_t audioBufSize = 0;
uint8_t  *audioBuffer = NULL;

void analyzeYUVData(jbyte *yData, jbyte *uData, jbyte *vData, jint rowStride, jint pixelStride);

void writeFrame();

void writeAudioFrame();

int initVideoCodecContext();

int initAudioCodecContext();

AVStream* initAvStream();

AVStream* initAudioStream();

};
#endif //NDKTEST_PRIVATEUTILS_H
