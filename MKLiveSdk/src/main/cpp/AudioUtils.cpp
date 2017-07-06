//
// Created by Mikiller on 2017/1/16.
//
#ifndef NDKTEST_AUDIOUTILS_CPP
#define NDKTEST_AUDIOUTILS_CPP

#include <pthread.h>
#include "AudioUtils.h"

int SAMPLERATE = 44100;
int duration = 1024 * 1000 / SAMPLERATE;

AVCodecContext *audioCodecCxt = NULL;
AVCodec *avAudioCodec = NULL;
AVFrame *audioFrame = NULL;
AVPacket avAudioPacket;
int audioStreamId;

AVCodecID getAudioCodecId() {
    if(!(avAudioCodec = avcodec_find_encoder_by_name("libfdk_aac"))){
        LOGE("init aac encoder failed");
        return AV_CODEC_ID_NONE;
    }
    avAudioCodec->capabilities |= AV_CODEC_CAP_VARIABLE_FRAME_SIZE;
    return avAudioCodec->id;
}

AVCodecContext *initAudioCodecContext(int bitRate, int channels) {
    if (!(audioCodecCxt = avcodec_alloc_context3(avAudioCodec))) {
        LOGE("init avAudioContext failed");
        return NULL;
    }
    LOGE("channels: %d", channels);
    audioCodecCxt->sample_fmt = avAudioCodec->sample_fmts[0];
    audioCodecCxt->sample_rate = SAMPLERATE;
    audioCodecCxt->channel_layout = av_get_default_channel_layout(channels);
    audioCodecCxt->channels = av_get_channel_layout_nb_channels(audioCodecCxt->channel_layout);
    audioCodecCxt->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    audioCodecCxt->bit_rate = bitRate;
    audioCodecCxt->time_base = {1, SAMPLERATE};
    audioCodecCxt->bits_per_raw_sample = 16;
    audioCodecCxt->flags |= CODEC_FLAG_GLOBAL_HEADER;

    return audioCodecCxt;
}

int openAudioEncoder(char* aacprofile_str) {
    AVDictionary *options = NULL;
    char profile[256];
    memset(profile, 0, 256);
    memcpy(profile, aacprofile_str, strlen(aacprofile_str));
    int ret = avcodec_open2(audioCodecCxt, avAudioCodec, &options);
    av_dict_free(&options);
    return ret;
}

int getAudioStreamId(AVFormatContext* outFormatCxt){
    AVStream *avStream = NULL;
    if (!(avStream = avformat_new_stream(outFormatCxt, avAudioCodec))) {
        LOGE("create audioStream failed");
        return -1;
    }
    avStream->time_base = audioCodecCxt->time_base;
    avcodec_parameters_from_context(avStream->codecpar, audioCodecCxt);
    return audioStreamId = avStream->index;
}

int initAvAudioFrame() {
    audioFrame = av_frame_alloc();
    audioFrame->format = audioCodecCxt->sample_fmt;
    audioFrame->sample_rate = audioCodecCxt->sample_rate;
    audioFrame->nb_samples = audioCodecCxt->frame_size;
    audioFrame->pts = 0;
    audioFrame->channels = audioCodecCxt->channels;
    audioFrame->channel_layout = audioCodecCxt->channel_layout;
    return av_frame_get_buffer(audioFrame, 0);
}

int encodeAudio(uint8_t *data, int needFrame){
    if(needFrame) {
        int audio_data_size_value = av_samples_get_buffer_size(NULL, audioCodecCxt->channels,
                                                               audioCodecCxt->frame_size,
                                                               audioCodecCxt->sample_fmt, 1);

        avcodec_fill_audio_frame(audioFrame, audioCodecCxt->channels, audioCodecCxt->sample_fmt,
                                 data, audio_data_size_value, 0);

        ret = avcodec_send_frame(audioCodecCxt, audioFrame);
        audioFrame->pts += audioFrame->nb_samples;
    }else{
        ret = avcodec_send_frame(audioCodecCxt, NULL);
        audioFrame->pts = 0;
    }
    if (ret < 0) {
        LOGError("avcodec_send_audio_frame error%d: %s", ret);
        av_packet_unref(&avAudioPacket);
        return ret;
    }
    av_init_packet(&avAudioPacket);
    avAudioPacket.data = NULL;
    avAudioPacket.size = 0;
    if ((ret = avcodec_receive_packet(audioCodecCxt, &avAudioPacket)) != 0) {
        LOGE("packet ret : %d", ret);
        av_packet_unref(&avAudioPacket);
    }
    avAudioPacket.flags |= AV_PKT_FLAG_KEY;
    avAudioPacket.stream_index = audioStreamId;
    avAudioPacket.duration = duration;
    return ret;
}

int writeAudioFrame(AVFormatContext *outFormatCxt, pthread_mutex_t *datalock, long long ts){
    if(avAudioPacket.pts != AV_NOPTS_VALUE){
        avAudioPacket.pts = TSCalculate() - ts;
        avAudioPacket.dts = avAudioPacket.pts;
    }
    pthread_mutex_lock(datalock);
    ret = av_interleaved_write_frame(outFormatCxt, &avAudioPacket);
    if (ret < 0) {
        LOGError("write audio frame ret: %d, %s", ret);
    }
    pthread_mutex_unlock(datalock);
    av_packet_unref(&avAudioPacket);
    return ret;
}


void flushAudio (AVFormatContext *outFormatCxt, pthread_mutex_t *datalock) {
    while (encodeAudio(NULL, false) == 0){
        writeAudioFrame(outFormatCxt, datalock, 0);
    }
}

void freeAudioReference() {
    if (audioCodecCxt != NULL && avcodec_is_open(audioCodecCxt)) {
        avcodec_free_context(&audioCodecCxt);
    }
    avAudioCodec = NULL;
    if (audioFrame != NULL)
        av_frame_free(&audioFrame);
}

#endif

