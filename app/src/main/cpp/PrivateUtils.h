//
// Created by Mikiller on 2016/12/8.
//

#ifndef NDKTEST_PRIVATEUTILS_H
#define NDKTEST_PRIVATEUTILS_H
extern "C"{
#include <libavformat/avformat.h>

AVFormatContext *outFormatCxt = NULL;
//AVStream *avStream = NULL;
AVCodecContext *pCodecCxt = NULL;
//use AVCodecParameters instead of AVCodecContext
//AVCodecParameters *pCodecParam = NULL;
AVCodec *avCodec = NULL;
AVPacket avPacket;
AVFrame *avFrame = NULL;
size_t bufferSize = 0;
uint8_t *frameBuffer = NULL;

void analyzeYUVData(jbyte *yData, jbyte *uData, jbyte *vData, jint rowStride, jint pixelStride);

void writeFrame();

int initCodecContext();

AVStream* initAvStream();
};
#endif //NDKTEST_PRIVATEUTILS_H
