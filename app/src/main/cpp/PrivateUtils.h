//
// Created by Mikiller on 2016/12/8.
//

#ifndef NDKTEST_PRIVATEUTILS_H
#define NDKTEST_PRIVATEUTILS_H


#include "jni.h"
extern "C"{
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include "libavutil/audio_fifo.h"

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
AVAudioFifo *fifo = NULL;
uint8_t ** outData = NULL;
uint8_t ** inputSample = NULL;
SwrContext * swrCxt = NULL;

AVCodecContext*pInputAudioCodecCxt = NULL;
AVFormatContext *inputFormatCxt = NULL;
char* inputUrl = NULL;
AVFrame * inFrame = NULL;
AVOutputFormat *fmt = NULL;
AVPacket inPkt;
FILE *inFile = NULL;
size_t aacPktSize;
uint8_t *aacBuf = NULL;


void analyzeYUVData(jbyte *yData, jbyte *uData, jbyte *vData, jint rowStride, jint pixelStride);

void writeFrame();

void writeAudioFrame();

int initVideoCodecContext();

int initAudioCodecContext();

AVStream* initAvStream();

AVStream* initAudioStream();

void initSwrContext();

int initAvAudioFrame(AVFrame **audioFrame, size_t frameSize);

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
