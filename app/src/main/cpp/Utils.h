//
// Created by Mikiller on 2017/1/4.
//
#ifndef NDKTEST_UTILS_H
#define NDKTEST_UTILS_H

#ifndef TAG
#define TAG "Utils.cpp"
#endif

#ifdef ANDROID

#include <jni.h>
#include <android/log.h>

#define LOGE(format, ...)  __android_log_print(ANDROID_LOG_ERROR, TAG, format, ##__VA_ARGS__)
#define LOGI(format, ...)  __android_log_print(ANDROID_LOG_INFO,  TAG, format, ##__VA_ARGS__)
#else
#define LOGE(format, ...)  printf(TAG format "\n", ##__VA_ARGS__)
#define LOGI(format, ...)  printf(TAG format "\n", ##__VA_ARGS__)
#endif

#include <cstdint>
#include "PrivateUtils.h"

extern "C" {
#include <libavutil/samplefmt.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>

AVCodecContext *pAudioCodecCxt = NULL;
AVCodec *avAudioCodec = NULL;
AVFrame *avAudioFrame = NULL;
AVPacket avAudioPacket;
AVAudioFifo *fifo = NULL;
uint8_t ** inputSample = NULL;

SwrContext * swrCxt = NULL;
uint64_t pts = 0;
int audioStreamId = 0;

static void LOGError(char *format, int ret) {
    static char error_buffer[255];
    av_strerror(ret, error_buffer, sizeof(error_buffer));
    LOGE(format, ret, error_buffer);
}

void initSwrContext();

/** Add converted input audio samples to the FIFO buffer for later processing. */
static int add_samples_to_fifo(AVAudioFifo *fifo,
                               uint8_t **input_samples,
                               const int input_frame_size) {
    int error;

    /**
     * Make the FIFO as large as it needs to be to hold both,
     * the old and the new samples.
     */
    if ((error = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + input_frame_size)) < 0) {
        LOGE("Could not reallocate FIFO\n");
        return error;
    }

    /** Store the new samples in the FIFO buffer. */
    if (av_audio_fifo_write(fifo, (void **) input_samples,
                            input_frame_size) < input_frame_size) {
        LOGE("Could not write data to FIFO\n");
        return AVERROR_EXIT;
    }
    return 0;
}

static int init_samples_buffer(uint8_t ***samples_buffer, int channels, AVSampleFormat sampleFmt,
                               int frameSize) {
    int error;

    /**
     * 分配足够的数组指针给sample buffer， 数组数量与声道数相关。
     */
    if (!(*samples_buffer = (uint8_t **) calloc(channels, sizeof(**samples_buffer)))) {
        LOGE("Could not allocate sample pointers\n");
        return AVERROR(ENOMEM);
    }

    /**
     * 为每个数组分配空间。
     */
    if ((error = av_samples_alloc(*samples_buffer, NULL, channels, frameSize, sampleFmt, 0)) < 0) {
        LOGError("Could not allocate converted input samples (error%d '%s')\n", error);
        av_freep(&(*samples_buffer)[0]);
        free(*samples_buffer);
        return error;
    }
    return 0;
}

static int readAndConvert(AVAudioFifo *fifo, uint8_t **inputSample, AVFrame *outAudioFrame){
    int ret = AVERROR_EXIT;
    const int frame_size = FFMIN(av_audio_fifo_size(fifo),
                                 pAudioCodecCxt->frame_size);
    if (pAudioCodecCxt->codec->capabilities & CODEC_CAP_VARIABLE_FRAME_SIZE){
        LOGE("aa");
    }
    for (int i = 0; i < inputChannels; i++) {
        memset(inputSample[i], 0, avAudioFrame->linesize[0]);
    }
    /**
     * 从fifo中一次读出nb_sample个数据
     */
    if ((ret = av_audio_fifo_read(fifo, (void **) inputSample, frame_size)) < frame_size) {
        LOGError("read fifo ret:%d, %s", ret);
        return ret;
    }
    /**
     * 复用avAudioFrame， 每次清除data和重设nb_samples
     */
    outAudioFrame->nb_samples = frame_size;
    for(int i = 0; i < outAudioFrame->channels; i++){
        memset(outAudioFrame->data[i], 0, outAudioFrame->linesize[0]);
    }
    ret = swr_convert(swrCxt, outAudioFrame->data, frame_size,
                      (const uint8_t **) inputSample, frame_size);
    if (ret < 0) {
        LOGError("Could not convert input samples (error%d '%s')\n", ret);
    }
    return ret;
}

static int encodeAudio(AVFrame *outAudioFrame, AVCodecContext *audioCodecCxt){
    int error;
    av_init_packet(&avAudioPacket);
    avAudioPacket.data = NULL;
    avAudioPacket.size = 0;

    /** Set a timestamp based on the sample rate for the container. */
    if (outAudioFrame) {
        outAudioFrame->pts = pts;
        pts += audioCodecCxt->frame_size;
    }

    /**
     * Encode the audio frame and store it in the temporary packet.
     * The output audio stream encoder is used to do this.
     */
    int data_present = 0;
    error = avcodec_send_frame(audioCodecCxt, avAudioFrame);
    if(error < 0){
        LOGError("avcodec_send_frame error%d: %s", error);
        return error;
    }
    int gotSample = avcodec_receive_packet(audioCodecCxt, &avAudioPacket);
    if(gotSample != 0){
        LOGError("avcodec_receive_packet (error%d '%s')\n", gotSample);
        av_packet_unref(&avAudioPacket);
    }
    return gotSample;
}

static int writeAudioFrame(AVFormatContext *outputFormat){
    int error = AVERROR_EXIT;
    avAudioPacket.stream_index = audioStreamId;
    av_packet_rescale_ts(&avAudioPacket, pAudioCodecCxt->time_base, outputFormat->streams[audioStreamId]->time_base);
    if ((error = av_interleaved_write_frame(outputFormat, &avAudioPacket)) < 0) {
        LOGE("audioStreamId:%d, audioPkt.pts:%lld", audioStreamId, avAudioPacket.pts);
        LOGError("Could not write frame (error%d '%s')\n", error);
    }
    av_packet_unref(&avAudioPacket);
    return error;
}

}

#endif //NDKTEST_UTILS_H