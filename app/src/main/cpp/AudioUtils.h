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

void initSampleParams(int ,int, int);

AVCodecID getAudioCodecId();

AVCodecContext* initAudioCodecContext();

int openAudioEncoder();

AVStream* initAudioStream(AVFormatContext*, int *);

int initAvAudioFrame();

void initSwrContext();

int init_samples_buffer();

int add_samples_to_fifo(uint8_t **, int);

int readAndConvert();

bool needWriteAudioFrame();

AVRational getAudioTimebase();

int encodeAudio(int64_t* audioPts);

int writeAudioFrame(AVFormatContext *outputFormat, int, int64_t);

void freeAudioReference();

}

#endif //NDKTEST_UTILS_H