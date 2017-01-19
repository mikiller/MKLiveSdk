//
// Created by Mikiller on 2017/1/16.
//
#ifndef NDKTEST_AUDIOUTILS_CPP
#define NDKTEST_AUDIOUTILS_CPP

#include "AudioUtils.h"

AVCodecContext *audioCodecCxt = NULL;
AVCodec *avAudioCodec = NULL;
AVFrame *audioFrame = NULL;
AVPacket avAudioPacket = { .data = NULL, .size = 0 };
AVRational audioTimebase = {1, 44100};
AVAudioFifo *fifo = NULL;
uint8_t **inputSample = NULL;
int inputChannels, inputSampleRate;
AVSampleFormat inputSampleFmt;

SwrContext *swrCxt = NULL;
uint64_t firstAudioTime = 0;


void initSampleParams(int channels, int sampleFmt, int sampleRate) {
    inputChannels = channels;
    switch (sampleFmt) {
        case 4:
            inputSampleFmt = AV_SAMPLE_FMT_FLT;
            break;
        default:
            inputSampleFmt = AV_SAMPLE_FMT_S16;
    }
    inputSampleRate = sampleRate;
}

AVCodecID getAudioCodecId() {
    if (!(avAudioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC))) {
        LOGE("init aac encoder failed");
        return AV_CODEC_ID_NONE;
    }
    return avAudioCodec->id;
}

AVCodecContext *initAudioCodecContext() {
    if (!(audioCodecCxt = avcodec_alloc_context3(avAudioCodec))) {
        LOGE("init avAudioContext failed");
        return NULL;
    }
    audioCodecCxt->sample_fmt = avAudioCodec->sample_fmts[0];
// audioCodecCxtxt->sample_rate = avAudioCodec->supported_samplerates[0];
    audioCodecCxt->sample_rate = 44100;
    audioCodecCxt->channel_layout = av_get_default_channel_layout(2);
    audioCodecCxt->channels = av_get_channel_layout_nb_channels(audioCodecCxt->channel_layout);
    audioCodecCxt->profile = FF_PROFILE_AAC_MAIN;
    audioCodecCxt->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    audioCodecCxt->bit_rate = 96 * 1000;
    audioCodecCxt->time_base = audioTimebase;
    return audioCodecCxt;
}

int openAudioEncoder() {
    return avcodec_open2(audioCodecCxt, avAudioCodec, NULL);
}

AVStream *initAudioStream(AVFormatContext *outFormatCxt, int *audioStreamId) {
    AVStream *avStream = NULL;
    if (!(avStream = avformat_new_stream(outFormatCxt, avAudioCodec))) {
//    if (!(avStream = avformat_new_stream(outFormatCxt, NULL))) {
        LOGE("create audioStream failed");
        ret = -1;
        return NULL;
    }
    avStream->codec = audioCodecCxt;
    avStream->time_base = audioCodecCxt->time_base;
    avcodec_parameters_from_context(avStream->codecpar, audioCodecCxt);
    *audioStreamId = avStream->index;
    return avStream;
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

void initSwrContext() {
    swrCxt = swr_alloc();
    fifo = av_audio_fifo_alloc(inputSampleFmt, inputChannels, 1);

#if LIBSWRESAMPLE_VERSION_MINOR >= 17
    av_opt_set_int(swrCxt, "inputChannel", inputChannels, 0);
    av_opt_set_int(swrCxt, "outputChannel", pAudioCodecCxt->channels, 0);
    av_opt_set_int(swrCxt, "inputSampleRate", inputSampleRate, 0);
    av_opt_set_int(swrCxt, "outputSampleRate", pAudioCodecCxt->sample_rate, 0);
    av_opt_set_sample_fmt(swrCxt, "inputFmt", inputSampleFmt, 0);
    av_opt_set_sample_fmt(swrCxt, "outputFmt", pAudioCodecCxt->sample_fmt, 0);
#else
    swrCxt = swr_alloc_set_opts(swrCxt,
                                audioCodecCxt->channel_layout,
                                audioCodecCxt->sample_fmt,
                                audioCodecCxt->sample_rate,
                                av_get_default_channel_layout(inputChannels),
                                inputSampleFmt,
                                inputSampleRate,
                                0, NULL);
#endif
    swr_init(swrCxt);
}

int init_samples_buffer() {
    int error;

    /**
     * 分配足够的数组指针给sample buffer， 数组数量与声道数相关。
     */
    if (!(inputSample = (uint8_t **) calloc(inputChannels, sizeof(*inputSample)))) {
        LOGE("Could not allocate sample pointers\n");
        return AVERROR(ENOMEM);
    }

    /**
     * 为每个数组分配空间。
     */
    if ((error = av_samples_alloc(inputSample, NULL, inputChannels, audioCodecCxt->frame_size,
                                  inputSampleFmt, 0)) < 0) {
        LOGError("Could not allocate converted input samples (error%d '%s')\n", error);
        av_freep(&inputSample[0]);
        free(inputSample);
        return error;
    }
    return 0;
}


/** Add converted input audio samples to the FIFO buffer for later processing. */
int add_samples_to_fifo(uint8_t **input_samples, int sampleLength) {
    int byteSize = inputSampleFmt == AV_SAMPLE_FMT_FLT ? sizeof(float) : sizeof(uint16_t);
    int input_frame_size = sampleLength / inputChannels / byteSize;
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

int readAndConvert() {
    int ret = AVERROR_EXIT;
    const int frame_size = FFMIN(av_audio_fifo_size(fifo),
                                 audioCodecCxt->frame_size);
    if (audioCodecCxt->codec->capabilities & CODEC_CAP_VARIABLE_FRAME_SIZE) {
        LOGE("aa");
    }
    for (int i = 0; i < inputChannels; i++) {
        memset(inputSample[i], 0, audioFrame->linesize[0]);
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
    audioFrame->nb_samples = frame_size;
    for (int i = 0; i < audioFrame->channels; i++) {
        memset(audioFrame->data[i], 0, audioFrame->linesize[0]);
    }
    ret = swr_convert(swrCxt, audioFrame->data, frame_size,
                      (const uint8_t **) inputSample, frame_size);
    if (ret < 0) {
        LOGError("Could not convert input samples (error%d '%s')\n", ret);
    }
    return ret;
}

int encodeAudio(int64_t *audioPts) {
    int error;
    av_init_packet(&avAudioPacket);
    avAudioPacket.data = NULL;
    avAudioPacket.size = 0;

    /** Set a timestamp based on the sample rate for the container. */
    if (audioFrame) {
        audioFrame->pts = (*audioPts += audioCodecCxt->frame_size);
//        pts += audioCodecCxt->frame_size;
    }

    /**
     * Encode the audio frame and store it in the temporary packet.
     * The output audio stream encoder is used to do this.
     */
    int data_present = 0;
    error = avcodec_send_frame(audioCodecCxt, audioFrame);
    if (error < 0) {
        LOGError("avcodec_send_audio_frame error%d: %s", error);
        return error;
    }
    int gotSample = avcodec_receive_packet(audioCodecCxt, &avAudioPacket);

    if (gotSample != 0) {
        av_packet_unref(&avAudioPacket);
    }
    return gotSample;
}

int writeAudioFrame(AVFormatContext *outputFormat, int audioStreamId, int64_t startTime) {
    int error = AVERROR_EXIT;
    avAudioPacket.stream_index = audioStreamId;
    av_packet_rescale_ts(&avAudioPacket, audioCodecCxt->time_base,
                         outputFormat->streams[audioStreamId]->time_base);
//    LOGE("audio pkt pts:%lld, dts:%lld", avAudioPacket.pts, avAudioPacket.dts);
    avAudioPacket.pts += firstDts * 2;
    avAudioPacket.dts += firstDts * 2;
    int64_t ptsTime = av_rescale_q(avAudioPacket.dts,
                                   outputFormat->streams[audioStreamId]->time_base,
                                   {1, AV_TIME_BASE});
    if(!firstAudioTime){
        firstAudioTime = (avAudioPacket.dts + startTime);
    }
    int64_t nowTime = av_gettime() - firstAudioTime;
    if (ptsTime > nowTime) {
        LOGE("audio sleeptime: %lld", ptsTime - nowTime);
        av_usleep(ptsTime - nowTime);
    }

    if ((error = av_interleaved_write_frame(outputFormat, &avAudioPacket)) < 0) {
        LOGError("Could not write frame (error%d '%s')\n", error);
    }
    av_packet_unref(&avAudioPacket);
    return error;
}

bool needWriteAudioFrame() {
    return av_audio_fifo_size(fifo) > audioCodecCxt->frame_size;
}

AVRational getAudioTimebase(){
    return audioTimebase;
}

void freeAudioReference() {
    if (audioCodecCxt != NULL) {
        avcodec_close(audioCodecCxt);
        avcodec_free_context(&audioCodecCxt);
        audioCodecCxt = NULL;
    }
    if (audioFrame != NULL) {
        av_frame_free(&audioFrame);
        audioFrame = NULL;
    }
    if (inputSample) {
        av_freep(inputSample);
        inputSample = NULL;
    }
}

#endif

