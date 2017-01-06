//
// Created by Mikiller on 2016/12/8.
//

#ifndef NDKTEST_PRIVATEUTILS_H
#define NDKTEST_PRIVATEUTILS_H

extern "C"{
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include "libavutil/audio_fifo.h"

AVFormatContext *outFormatCxt = NULL;
int inputChannels, inputSampleRate;
AVSampleFormat inputSampleFmt;

AVCodecContext *pVideoCodecCxt = NULL;
AVCodec *avVideoCodec = NULL;
AVPacket avVideoPacket;
AVFrame *avVideoFrame = NULL;
int videoStreamId = 0;

void analyzeYUVData(uint8_t *yData, uint8_t *uData, uint8_t *vData, int rowStride, int pixelStride);

int encodeYUV(AVPacket *avVideoPacket);

void writeVideoFrame(AVPacket *avVideoPacket);

//void writeFrame();

//void writeAudioFrame();

int initVideoCodecContext();

int initAudioCodecContext();

AVStream* initAvStream();

AVStream* initAudioStream();

int initAvAudioFrame(AVFrame **audioFrame);

int initAvVideoFrame(AVFrame **videoFrame);

static av_always_inline int64_t ff_samples_to_time_base(AVCodecContext *avctx,
                                                        int64_t samples)
{
    if(samples == AV_NOPTS_VALUE)
        return AV_NOPTS_VALUE;
    return av_rescale_q(samples, (AVRational){ 1, avctx->sample_rate },
                        avctx->time_base);
}

};
#endif //NDKTEST_PRIVATEUTILS_H
