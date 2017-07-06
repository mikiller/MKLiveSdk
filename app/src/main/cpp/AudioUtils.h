//
// Created by Mikiller on 2017/1/4.
//
#ifndef NDKTEST_UTILS_H
#define NDKTEST_UTILS_H

#define TAG "AUDIO_UTILS"

//#include <cstdint>
extern "C" {
#include "PrivateUtils.h"
#include <libavutil/samplefmt.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>

AVCodecID getAudioCodecId();

AVCodecContext* initAudioCodecContext(int, int);

int openAudioEncoder(char* aacprofile);

int getAudioStreamId(AVFormatContext*);

int initAvAudioFrame();

int encodeAudio(uint8_t * , int needFrame = true);

int writeAudioFrame(AVFormatContext *, pthread_mutex_t *, long long);

void flushAudio(AVFormatContext *, pthread_mutex_t *);

void freeAudioReference();

}

#endif //NDKTEST_UTILS_H