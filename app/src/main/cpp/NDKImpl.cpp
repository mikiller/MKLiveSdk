//
// Created by Mikiller on 2016/11/25.
//
#ifndef TAG
#define TAG "NDKImpl.cpp"
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

#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include "NDKImpl.h"
#include "PrivateUtils.h"


extern "C" {
#include "libavutil/log.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libavutil/imgutils.h"
#include "libavutil/time.h"
#include "libavutil/fifo.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
#include "libavdevice/avdevice.h"
#include "libpostproc/postprocess.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavutil/avassert.h"


JNIEXPORT jstring JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_helloWorld(JNIEnv *env, jobject instance,
                                                            jstring input_) {
    const char *input = env->GetStringUTFChars(input_, 0);

    // TODO

    env->ReleaseStringUTFChars(input_, input);

    return env->NewStringUTF(input);
}

void custom_log(void *ptr, int level, const char *fmt, va_list vl) {
    FILE *file = fopen("/storage/emulated/0/av_log.txt", "a+");
    if (file) {
        vfprintf(file, fmt, vl);
        fflush(file);
        fclose(file);
    }
}


//AVFormatContext *outFormatCxt = NULL;
//AVStream *avStream = NULL;
//AVCodecContext *pVideoCodecCxt = NULL;
////use AVCodecParameters instead of AVCodecContext
//AVCodecParameters *pCodecParam = NULL;
//AVCodec *avVideoCodec = NULL;
//AVPacket avVideoPacket;
//AVFrame *avVideoFrame = NULL;
pthread_mutex_t lock;

int ret = 0;
int framecnt = 2;
int yuvWidth = 0;
int yuvHeight = 0;
int yLength = 0;
int uvLenght = 0;
int64_t startTime = 0;
int64_t audioStartTime = 0;

int audiocnt = 0;

void LOGError(char* format, int ret){
    static char error_buffer[255];
    av_strerror(ret, error_buffer, sizeof(error_buffer));
    LOGE(format, ret, error_buffer);
}

jint end(AVFormatContext **inputFormatContext, AVFormatContext *outputFormatContext) {
    if (inputFormatContext != NULL)
        avformat_close_input(inputFormatContext);
    if (outputFormatContext) {
//        if(outputFormatContext->oformat && !(outputFormatContext->oformat->flags & AVFMT_NOFILE)){
//            avio_close(outputFormatContext->pb);
//        }
        avformat_free_context(outputFormatContext);
    }

    if (pVideoCodecCxt != NULL) {
        avcodec_close(pVideoCodecCxt);
        avcodec_free_context(&pVideoCodecCxt);
    }
    if (avVideoFrame != NULL)
        av_frame_free(&avVideoFrame);
    if (ret < 0 && ret == AVERROR_EOF) {
        LOGE("unknow error");
        return -1;
    }
    pthread_mutex_destroy(&lock);
    startTime = 0;
    framecnt = 2;
    audiocnt = 0;
    yLength = 0;
    uvLenght = 0;
    return ret;
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_pushRTMP(JNIEnv *env, jclass type,
                                                          jstring input_, jstring output_) {
    const char *input = env->GetStringUTFChars(input_, 0);
    const char *output = env->GetStringUTFChars(output_, 0);

    // TODO
    LOGE("%s", input);
    LOGE("%s", output);

    AVFormatContext *inputFormatContext = NULL, *outputFormatContext = NULL;
    AVPacket packet;

    int i;

    av_log_set_callback(custom_log);

    av_register_all();

    avdevice_register_all();

    avformat_network_init();

    avformat_alloc_output_context2(&outputFormatContext, NULL, "flv", output);
    if (!outputFormatContext) {
        LOGE("create output failed");
        ret = -1;
        return end(&inputFormatContext, outputFormatContext);
    }
    //input from file
    if ((ret = avformat_open_input(&inputFormatContext, input, 0, 0)) < 0) {
        LOGE("input file isnot exist");
        return end(&inputFormatContext, outputFormatContext);
    }

    if ((ret = avformat_find_stream_info(inputFormatContext, 0)) < 0) {
        LOGE("failed to get input information");
        return end(&inputFormatContext, outputFormatContext);
    }

    int videoId = -1;
    for (i = 0; i < inputFormatContext->nb_streams; i++) {
        if (inputFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoId = i;
            break;
        }
    }

    for (i = 0; i < inputFormatContext->nb_streams; i++) {
        AVStream *inputStream = inputFormatContext->streams[i];
        //codec is null?
        AVStream *outputStream = avformat_new_stream(outputFormatContext,
                                                     inputStream->codec->codec);
        if (!outputStream) {
            LOGE("allocate outputStream failed");
            ret = -1;
            return end(&inputFormatContext, outputFormatContext);
        }
//        ret = avcodec_copy_context(outputStream->codec, inputStream->codec);
        ret = avcodec_parameters_copy(outputStream->codecpar, inputStream->codecpar);
        //        ret = avcodec_parameters_from_context(outputStream->codecpar, inputStream->codec);
        if (ret < 0) {
            LOGE("copy faield");
            ret = -1;
            return end(&inputFormatContext, outputFormatContext);
        }
        outputStream->codecpar->codec_tag = 0;
        if (outputFormatContext->oformat->flags & AVFMT_GLOBALHEADER) {
            outputStream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }
    }
    if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&outputFormatContext->pb, output, AVIO_FLAG_WRITE);
        if (ret < 0) {
            LOGE("could not open url: %s", output);
            ret = -1;
            return end(&inputFormatContext, outputFormatContext);
        }
    }

    ret = avformat_write_header(outputFormatContext, NULL);
    if (ret < 0) {
        LOGE("write open url head failed");
        ret = -1;
        return end(&inputFormatContext, outputFormatContext);
    }

    int frame_index = 0;

    int64_t start_time = av_gettime();
    while (1) {
        AVStream *inputStream, *outputStream;
        ret = av_read_frame(inputFormatContext, &packet);
        if (ret < 0)
            break;
        if (packet.pts == AV_NOPTS_VALUE) {
            AVRational timeBase1 = inputFormatContext->streams[videoId]->time_base;
            int64_t duration = (double) AV_TIME_BASE /
                               av_q2d(inputFormatContext->streams[videoId]->r_frame_rate);
            packet.pts =
                    (double) (frame_index * duration) / (double) (av_q2d(timeBase1) * AV_TIME_BASE);
            packet.dts = packet.pts;
            packet.duration = (double) duration / (double) (av_q2d(timeBase1) * AV_TIME_BASE);
        }

        if (packet.stream_index == videoId) {
            AVRational timeBase1 = inputFormatContext->streams[videoId]->time_base;
            AVRational base = {1, AV_TIME_BASE};
            int64_t ptsTime = av_rescale_q(packet.dts, timeBase1, base);
            int64_t nowTime = av_gettime() - start_time;
            if (ptsTime > nowTime) {
                av_usleep(ptsTime - nowTime);
            }
        }

        inputStream = inputFormatContext->streams[packet.stream_index];
        outputStream = outputFormatContext->streams[packet.stream_index];

        packet.pts = av_rescale_q_rnd(packet.pts, inputStream->time_base, outputStream->time_base,
                                      (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet.dts = av_rescale_q_rnd(packet.dts, inputStream->time_base, outputStream->time_base,
                                      (AVRounding) (AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
        packet.duration = av_rescale_q(packet.duration, inputStream->time_base,
                                       outputStream->time_base);
        packet.pos = -1;

        if (packet.stream_index == videoId) {
            LOGE("send video id: %d", frame_index);
            frame_index++;
        }

        if ((ret = av_interleaved_write_frame(outputFormatContext, &packet)) < 0) {
            LOGE("error in muxing fram");
            continue;
        }
        av_packet_unref(&packet);
    }

    av_write_trailer(outputFormatContext);

    env->ReleaseStringUTFChars(input_, input);
    env->ReleaseStringUTFChars(output_, output);
    int rst = end(&inputFormatContext, outputFormatContext);
    return rst;
}

static void registFFMpeg(){
    av_log_set_callback(custom_log);

    av_register_all();
    avcodec_register_all();

    avformat_network_init();
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_initFFMpeg(JNIEnv *env, jclass type,
                                                            jstring outputUrl_, jint width,
                                                            jint height) {
    const char *outputUrl = env->GetStringUTFChars(outputUrl_, 0);

    // TODO
    yuvWidth = width, yuvHeight = height;
    yLength = width * height;
    uvLenght = yLength / 4;

//    av_log_set_callback(custom_log);
//
//    av_register_all();
//    avcodec_register_all();
//
//    avformat_network_init();

    avformat_alloc_output_context2(&outFormatCxt, NULL, NULL, outputUrl);
    fmt = outFormatCxt->oformat;
    if ((ret = avio_open(&outFormatCxt->pb, outputUrl, AVIO_FLAG_WRITE)) < 0) {
        LOGE("failed to open outputUrl: %s", outputUrl);
        return end(NULL, outFormatCxt);
    }

//    if (!(avVideoCodec = avcodec_find_encoder(AV_CODEC_ID_H264))) {
//        LOGE("init h264 encoder failed");
//        ret = -1;
//        return end(NULL, outFormatCxt);
//    }


    if (!(avAudioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC))) {
//    if (!(avAudioCodec = avcodec_find_encoder_by_name("libfdk_aac"))) {
        LOGE("init aac encoder failed");
        ret = -1;
        return end(NULL, outFormatCxt);
    }

//    outFormatCxt->oformat->video_codec = avVideoCodec->id;
    //outFormatCxt->oformat->audio_codec = avAudioCodec->id;

//    if (initVideoCodecContext() < 0)
//        return end(NULL, outFormatCxt);

    if (initAudioCodecContext() < 0) {
        return end(NULL, outFormatCxt);
    }

    if (outFormatCxt->oformat->flags & AVFMT_GLOBALHEADER){
//        pVideoCodecCxt->flags |= CODEC_FLAG_GLOBAL_HEADER;
        pAudioCodecCxt->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

//    AVDictionary *param = NULL;
//    av_dict_set(&param, "preset", "ultrafast"/*"slow"*/, 0);
//    av_dict_set(&param, "tune", "zerolatency", 0);
//    if ((ret = avcodec_open2(pVideoCodecCxt, avVideoCodec, &param)) < 0) {
//        LOGE("open encoder failed");
//        return end(NULL, outFormatCxt);
//    }

    if ((ret = avcodec_open2(pAudioCodecCxt, avAudioCodec, NULL)) < 0) {
        LOGE("open encoder failed %s", av_err2str(ret));
        return end(NULL, outFormatCxt);
    }

//    AVStream *avStream = initAvStream();
//    if (!avStream) {
//        return end(NULL, outFormatCxt);
//    }
    AVStream *audioStream = initAudioStream();
    if (!audioStream) {
        return end(NULL, outFormatCxt);
    }

    avformat_write_header(outFormatCxt, NULL);
    //avStream->time_base.den = 900;
//    startTime = av_gettime();

//    avVideoFrame = av_frame_alloc();
//    frameBufSize = av_image_get_buffer_size(pVideoCodecCxt->pix_fmt, yuvWidth, yuvHeight, 1);
//    frameBuffer = (uint8_t *) av_malloc(frameBufSize);

    initSwrContext();
    pthread_mutex_init(&lock, NULL);

    //avAudioFrame->pts = 0;
    audioBufSize = av_samples_get_buffer_size(NULL, pAudioCodecCxt->channels,
                                              pAudioCodecCxt->frame_size,
                                              /*pAudioCodecCxt->sample_fmt*/AV_SAMPLE_FMT_S16, 0);
    audioBuffer = (uint8_t *) av_malloc(audioBufSize);
    aacPktSize = av_samples_get_buffer_size(NULL, pAudioCodecCxt->channels,
                                                   pAudioCodecCxt->frame_size,
                                                   pAudioCodecCxt->sample_fmt, 0);
    aacBuf = (uint8_t*)av_malloc(aacPktSize);
    av_new_packet(&avAudioPacket, aacPktSize);

    inputSample = (uint8_t **) calloc(1, sizeof(uint8_t));

    outData = (uint8_t **) calloc(pAudioCodecCxt->channels, sizeof(uint8_t*));
    av_samples_alloc(outData, NULL, pAudioCodecCxt->channels, pAudioCodecCxt->frame_size, pAudioCodecCxt->sample_fmt, 0);
//    initAvAudioFrame();

    env->ReleaseStringUTFChars(outputUrl_, outputUrl);
    return 0;
}

static int open_input_file(const char *filename,
                           AVFormatContext **input_format_context,
                           AVCodecContext **input_codec_context)
{
    AVCodecContext *avctx;
    AVCodec *input_codec;
    int error;

    /** Open the input file to read from it. */
    if ((error = avformat_open_input(input_format_context, filename, NULL,
                                     NULL)) < 0) {
//        fprintf(stderr, "Could not open input file '%s' (error '%s')\n",
//                filename, get_error_text(error));
        LOGError("Could not open input file %d (error '%s')\n", error);
        *input_format_context = NULL;
        return error;
    }

    /** Get information on the input file (number of streams etc.). */
    if ((error = avformat_find_stream_info(*input_format_context, NULL)) < 0) {
//        fprintf(stderr, "Could not open find stream info (error '%s')\n",
//                get_error_text(error));
        LOGError("Could not open find stream info (error '%s')\n", error);
        avformat_close_input(input_format_context);
        return error;
    }

    /** Make sure that there is only one stream in the input file. */
//    if ((*input_format_context)->nb_streams != 1) {
//        fprintf(stderr, "Expected one audio input stream, but found %d\n",
//                (*input_format_context)->nb_streams);
//        avformat_close_input(input_format_context);
//        return AVERROR_EXIT;
//    }

    /** Find a decoder for the audio stream. */
    if (!(input_codec = avcodec_find_decoder((*input_format_context)->streams[0]->codecpar->codec_id))) {
//        fprintf(stderr, "Could not find input codec\n");
        LOGE("Could not find input codec\n");
        avformat_close_input(input_format_context);
        return AVERROR_EXIT;
    }

    /** allocate a new decoding context */
    avctx = avcodec_alloc_context3(input_codec);
    if (!avctx) {
//        fprintf(stderr, "Could not allocate a decoding context\n");
        LOGE("Could not allocate a decoding context\n");
        avformat_close_input(input_format_context);
        return AVERROR(ENOMEM);
    }

    /** initialize the stream parameters with demuxer information */
    error = avcodec_parameters_to_context(avctx, (*input_format_context)->streams[0]->codecpar);
    if (error < 0) {
        LOGError("copy codecpar error:%d, %s", error);
        avformat_close_input(input_format_context);
        avcodec_free_context(&avctx);
        return error;
    }
    avctx->channel_layout = av_get_default_channel_layout(avctx->channels);

    /** Open the decoder for the audio stream to use it later. */
    if ((error = avcodec_open2(avctx, input_codec, NULL)) < 0) {
//        fprintf(stderr, "Could not open input codec (error '%s')\n",
//                get_error_text(error));
        LOGError("Could not open input codec (error%d '%s')\n",ret);
        avcodec_free_context(&avctx);
        avformat_close_input(input_format_context);
        return error;
    }

    /** Save the decoder context for easier access later. */
    *input_codec_context = avctx;

    return 0;
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_initFFMpeg2(JNIEnv *env, jclass type,
                                                            jstring outputUrl_, jstring inputUrl_, jint width,
                                                            jint height) {
    inputUrl = (char *) env->GetStringUTFChars(inputUrl_, 0);
    registFFMpeg();
    open_input_file(inputUrl, &inputFormatCxt, &pInputAudioCodecCxt);
    inFile = fopen(inputUrl, "rb");
    Java_com_mikiller_ndktest_ndkapplication_NDKImpl_initFFMpeg(env, type, outputUrl_, width, height);
    env->ReleaseStringUTFChars(inputUrl_, inputUrl);
    return 0;
}

int initVideoCodecContext() {
    if (!(pVideoCodecCxt = avcodec_alloc_context3(avVideoCodec))) {
        LOGE("init avVideoContext failed");
        return -1;
    }
//    pVideoCodecCxt->codec_id = outFormatCxt->oformat->video_codec;
//    pVideoCodecCxt->codec_type = AVMEDIA_TYPE_VIDEO;
    pVideoCodecCxt->pix_fmt = AV_PIX_FMT_YUV420P;
    pVideoCodecCxt->width = yuvWidth;
    pVideoCodecCxt->height = yuvHeight;
    pVideoCodecCxt->time_base.num = 1;
    pVideoCodecCxt->time_base.den = 25;
    pVideoCodecCxt->bit_rate = 500 * 1000; //传输速率 rate/kbps
    pVideoCodecCxt->gop_size = 12; //gop = fps/N ?
    pVideoCodecCxt->max_b_frames = 23; // fps?
    pVideoCodecCxt->qmin = 10; // 1-51, 10-30 is better
    pVideoCodecCxt->qmax = 30; // 1-51, 10-30 is better
    pVideoCodecCxt->profile = FF_PROFILE_H264_MAIN;
    return 0;
}

int initAudioCodecContext() {
    if (!(pAudioCodecCxt = avcodec_alloc_context3(avAudioCodec))) {
        LOGE("init avAudioContext failed");
        return AVERROR(ENOMEM);
    }

//    pAudioCodecCxt->codec_id = fmt->audio_codec;
//    pAudioCodecCxt->codec_type = AVMEDIA_TYPE_AUDIO;
    pAudioCodecCxt->sample_fmt = avAudioCodec->sample_fmts[0];
    pAudioCodecCxt->sample_rate = pInputAudioCodecCxt->sample_rate;
    pAudioCodecCxt->channel_layout = av_get_default_channel_layout(2);
    pAudioCodecCxt->channels = av_get_channel_layout_nb_channels(pAudioCodecCxt->channel_layout);
    pAudioCodecCxt->profile = FF_PROFILE_AAC_MAIN;
    pAudioCodecCxt->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    pAudioCodecCxt->bit_rate = 96 * 1000;
    pAudioCodecCxt->time_base = {1, pAudioCodecCxt->sample_rate};

    return 0;
}

AVStream *initAvStream() {
    AVStream *avStream = NULL;
    if (!(avStream = avformat_new_stream(outFormatCxt, avVideoCodec))) {
        LOGE("create avStream failed");
        ret = -1;
        return NULL;
    }
    avcodec_parameters_from_context(avStream->codecpar, pVideoCodecCxt);
    return avStream;
}

AVStream *initAudioStream() {
    AVStream *avStream = NULL;
//    if (!(avStream = avformat_new_stream(outFormatCxt, avAudioCodec))) {
    if (!(avStream = avformat_new_stream(outFormatCxt, NULL))) {
        LOGE("create audioStream failed");
        ret = -1;
        return NULL;
    }
    avcodec_parameters_from_context(avStream->codecpar, pAudioCodecCxt);
    return avStream;
}

int initAvAudioFrame(AVFrame **avAudioFrame, size_t frameSize){
    *avAudioFrame = av_frame_alloc();
    (*avAudioFrame)->format = pAudioCodecCxt->sample_fmt;
    (*avAudioFrame)->sample_rate = pAudioCodecCxt->sample_rate;
    (*avAudioFrame)->nb_samples = frameSize;
//    avAudioFrame->pts = av_rescale_q(audiocnt+=avAudioFrame->nb_samples, pAudioCodecCxt->time_base, outFormatCxt->streams[1]->time_base);
    (*avAudioFrame)->pts = 0;
    (*avAudioFrame)->channels = pAudioCodecCxt->channels;
    (*avAudioFrame)->channel_layout = pAudioCodecCxt->channel_layout;
    av_frame_get_buffer((*avAudioFrame), 0);
    return 0;
//    avcodec_fill_audio_frame(avAudioFrame, pAudioCodecCxt->channels, pAudioCodecCxt->sample_fmt,
//                             aacBuf, aacPktSize, 2);
}

void initSwrContext(){
//    AVFifoBuffer **fifo = NULL;
    swrCxt = swr_alloc();

    fifo = av_audio_fifo_alloc(pAudioCodecCxt->sample_fmt, pAudioCodecCxt->channels, 1);

#if LIBSWRESAMPLE_VERSION_MINOR >= 17
    av_opt_set_int(swrCxt, "inputChannel", pAudioCodecCxt->channels, 0);
    av_opt_set_int(swrCxt, "outputChannel", pAudioCodecCxt->channels, 0);
    av_opt_set_int(swrCxt, "inputSampleRate", pAudioCodecCxt->sample_rate, 0);
    av_opt_set_int(swrCxt, "outputSampleRate", pAudioCodecCxt->sample_rate, 0);
    av_opt_set_sample_fmt(swrCxt, "inputFmt", AV_SAMPLE_FMT_FLT, 0);
    av_opt_set_sample_fmt(swrCxt, "outputFmt", pAudioCodecCxt->sample_fmt, 0);
#else
    swrCxt = swr_alloc_set_opts(swrCxt,
                                pAudioCodecCxt->channel_layout,
                                pAudioCodecCxt->sample_fmt,
                                pAudioCodecCxt->sample_rate,
                                pInputAudioCodecCxt->channel_layout,
                                pInputAudioCodecCxt->sample_fmt,
                                pInputAudioCodecCxt->sample_rate,
                                0, NULL);
#endif
    swr_init(swrCxt);
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_encodeYUV(JNIEnv *env, jclass type,
                                                           jbyteArray yuvData_) {
    jbyte *yuvData = env->GetByteArrayElements(yuvData_, NULL);

    // TODO

    if (pVideoCodecCxt == NULL) {
        ret = -1;
        return end(NULL, outFormatCxt);
    }

    av_image_fill_arrays(avVideoFrame->data, avVideoFrame->linesize, frameBuffer,
                         pVideoCodecCxt->pix_fmt,
                         yuvWidth, yuvHeight, 1);

    //convert android camera data from NV21 to yuv420p
    memcpy(avVideoFrame->data[0], yuvData, yLength);
    for (int i = 0; i < uvLenght; i++) {
        *(avVideoFrame->data[2] + i) = *(yuvData + yLength + i * 2);
        *(avVideoFrame->data[1] + i) = *(yuvData + yLength + i * 2 + 1);
    }

    writeFrame();
    env->ReleaseByteArrayElements(yuvData_, yuvData, 0);
    return ret;
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_encodeYUV1(JNIEnv *env, jclass type,
                                                            jbyteArray yData_, jbyteArray uData_,
                                                            jbyteArray vData_,
                                                            jint rowStride, jint pixelStride) {
    jbyte *yData = env->GetByteArrayElements(yData_, NULL);
    jbyte *uData = env->GetByteArrayElements(uData_, NULL);
    jbyte *vData = env->GetByteArrayElements(vData_, NULL);

    // TODO


    if (pVideoCodecCxt == NULL) {
        ret = -1;
        return end(NULL, outFormatCxt);
    }
    memset(frameBuffer, 0, frameBufSize);
    av_image_fill_arrays(avVideoFrame->data, avVideoFrame->linesize, frameBuffer,
                         pVideoCodecCxt->pix_fmt,
                         yuvWidth, yuvHeight, 1);

    analyzeYUVData(yData, uData, vData, rowStride, pixelStride);
    writeFrame();
    env->ReleaseByteArrayElements(yData_, yData, 0);
    env->ReleaseByteArrayElements(uData_, uData, 0);
    env->ReleaseByteArrayElements(vData_, vData, 0);
    return ret;
}

void analyzeYUVData(jbyte *y, jbyte *u, jbyte *v, jint rowStride, jint pixelStride) {
    int offset = 0;
    int length = yuvWidth;
    for (int row = 0; row < yuvHeight; row++) {
        memcpy((avVideoFrame->data[0] + length), y, yuvWidth);
        length += yuvWidth;
        y += rowStride;
        if (row < yuvHeight >> 1) {
            for (int col = 0; col < yuvWidth >> 1; col++) {
                *(avVideoFrame->data[1] + offset) = *(u + col * pixelStride);
                *(avVideoFrame->data[2] + offset++) = *(v + col * pixelStride);
            }
            u += rowStride;
            v += rowStride;
        }
    }
//    framecnt++;
//    avVideoFrame->pts = framecnt;
}

void writeFrame() {
    avVideoFrame->format = AV_PIX_FMT_YUV420P;
    avVideoFrame->width = yuvWidth;
    avVideoFrame->height = yuvHeight;
    avVideoFrame->pts = ++framecnt; /* * 1000 / 25*/;

    avVideoPacket.data = NULL;
    avVideoPacket.size = 0;
    av_init_packet(&avVideoPacket);
//    int g = 0;
//    int encGotFrame = avcodec_encode_video2(pVideoCodecCxt, &avVideoPacket, avVideoFrame, &g);
    ret = avcodec_send_frame(pVideoCodecCxt, avVideoFrame);
    int encGotFrame = avcodec_receive_packet(pVideoCodecCxt, &avVideoPacket);

    if (encGotFrame == 0) {
        avVideoPacket.stream_index = outFormatCxt->streams[0]->index;
        AVRational frameRate = {25, 1};
        avVideoPacket.pts = av_rescale(avVideoPacket.pts, outFormatCxt->streams[0]->time_base.den,
                                       20);
        avVideoPacket.dts = av_rescale(avVideoPacket.dts, outFormatCxt->streams[0]->time_base.den,
                                       20);
        avVideoPacket.duration =
                (AV_TIME_BASE) / av_q2d(frameRate) / outFormatCxt->streams[0]->time_base.den;
        avVideoPacket.pos = -1;
//        LOGE("NDK pkt pts: %lld, dts: %lld", avVideoPacket.pts, avVideoPacket.dts);
        int64_t ptsTime = av_rescale_q(avVideoPacket.dts, outFormatCxt->streams[0]->time_base,
                                       (AVRational) {1, 25});
        int64_t nowTime = av_gettime() - startTime;
//        LOGE("starttime1: %lld", startTime);
//        LOGE("ptstime: %lld, nowtime: %lld", ptsTime, nowTime);
        if (ptsTime > nowTime) {
            LOGE("sleeptime: %lld", ptsTime - nowTime);
            av_usleep(ptsTime - nowTime);
        }
        pthread_mutex_lock(&lock);
        LOGE("video packet dts:%lld", avVideoPacket.dts);
//        ret = av_interleaved_write_frame(outFormatCxt, &avVideoPacket);
        ret = av_write_frame(outFormatCxt, &avVideoPacket);
        if(ret < 0){
            static char error_buffer[255];
            av_strerror(ret, error_buffer, sizeof(error_buffer));
            LOGE("write video frame ret: %d, %s", ret, error_buffer);
        }
        pthread_mutex_unlock(&lock);
        av_packet_unref(&avVideoPacket);
        //av_frame_free(&avVideoFrame);
    }
}

/** Decode one audio frame from the input file. */
static int decode_audio_frame(AVFrame *frame,
                              AVFormatContext *input_format_context,
                              AVCodecContext *input_codec_context,
                              int *data_present, int *finished)
{
    /** Packet used for temporary storage. */
    AVPacket input_packet;
    int error;
    av_init_packet(&input_packet);
    input_packet.data = NULL;
    input_packet.size = 0;

    /** Read one audio frame from the input file into a temporary packet. */
    if ((error = av_read_frame(input_format_context, &input_packet)) < 0) {
        /** If we are at the end of the file, flush the decoder below. */
        if (error == AVERROR_EOF)
            *finished = 1;
        else {
//            fprintf(stderr, "Could not read frame (error '%s')\n",
//                    get_error_text(error));
            LOGError("Could not read frame (error%d '%s')\n", error);
            return error;
        }
    }

    /**
     * Decode the audio frame stored in the temporary packet.
     * The input audio stream decoder is used to do this.
     * If we are at the end of the file, pass an empty packet to the decoder
     * to flush it.
     */
    if ((error = avcodec_decode_audio4(input_codec_context, frame,
                                       data_present, &input_packet)) < 0) {
//        fprintf(stderr, "Could not decode frame (error '%s')\n",
//                get_error_text(error));
        LOGError("Could not decode frame (error%d '%s')\n", error);
        av_packet_unref(&input_packet);
        return error;
    }



    /**
     * If the decoder has not been flushed completely, we are not finished,
     * so that this function has to be called again.
     */
    if (*finished && *data_present)
        *finished = 0;
    av_packet_unref(&input_packet);
    return 0;
}

/**
 * Initialize a temporary storage for the specified number of audio samples.
 * The conversion requires temporary storage due to the different format.
 * The number of audio samples to be allocated is specified in frame_size.
 */
static int init_converted_samples(uint8_t ***converted_input_samples,
                                  AVCodecContext *output_codec_context,
                                  int frame_size)
{
    int error;

    /**
     * Allocate as many pointers as there are audio channels.
     * Each pointer will later point to the audio samples of the corresponding
     * channels (although it may be NULL for interleaved formats).
     */
    if (!(*converted_input_samples = (uint8_t **) calloc(output_codec_context->channels,
                                                         sizeof(**converted_input_samples)))) {
        fprintf(stderr, "Could not allocate converted input sample pointers\n");
        return AVERROR(ENOMEM);
    }

    /**
     * Allocate memory for the samples of all channels in one consecutive
     * block for convenience.
     */
    if ((error = av_samples_alloc(*converted_input_samples, NULL,
                                  output_codec_context->channels,
                                  frame_size,
                                  output_codec_context->sample_fmt, 0)) < 0) {
//        fprintf(stderr,
//                "Could not allocate converted input samples (error '%s')\n",
//                get_error_text(error));
        LOGError("Could not allocate converted input samples (error%d '%s')\n", error);
        av_freep(&(*converted_input_samples)[0]);
        free(*converted_input_samples);
        return error;
    }
    return 0;
}

/** Add converted input audio samples to the FIFO buffer for later processing. */
static int add_samples_to_fifo(AVAudioFifo *fifo,
                               uint8_t **converted_input_samples,
                               const int frame_size)
{
    int error;

    /**
     * Make the FIFO as large as it needs to be to hold both,
     * the old and the new samples.
     */
    if ((error = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + frame_size)) < 0) {
//        fprintf(stderr, "Could not reallocate FIFO\n");
        LOGE("Could not reallocate FIFO\n");
        return error;
    }

    /** Store the new samples in the FIFO buffer. */
    if (av_audio_fifo_write(fifo, (void **)converted_input_samples,
                            frame_size) < frame_size) {
//        fprintf(stderr, "Could not write data to FIFO\n");
        LOGE("Could not write data to FIFO\n");
        return AVERROR_EXIT;
    }
    return 0;
}

static int read_decode_convert_and_store(AVAudioFifo *fifo,
                                         AVFormatContext *input_format_context,
                                         AVCodecContext *input_codec_context,
                                         AVCodecContext *output_codec_context,
                                         SwrContext *resampler_context,
                                         int *finished)
{
    /** Temporary storage of the input samples of the frame read from the file. */
    AVFrame *input_frame = NULL;
    /** Temporary storage for the converted input samples. */
    uint8_t **converted_input_samples = NULL;
    int data_present;
    int ret = AVERROR_EXIT;

    /** Initialize temporary storage for one input frame. */
    if ((input_frame = av_frame_alloc()) ==NULL)
        goto cleanup;
    /** Decode one frame worth of audio samples. */
    if (decode_audio_frame(input_frame, input_format_context,
                           input_codec_context, &data_present, finished))
        goto cleanup;
    /**
     * If we are at the end of the file and there are no more samples
     * in the decoder which are delayed, we are actually finished.
     * This must not be treated as an error.
     */
    if (*finished && !data_present) {
        ret = 0;
        goto cleanup;
    }
    /** If there is decoded data, convert and store it */
    if (data_present) {
        /** Initialize the temporary storage for the converted input samples. */
        if (init_converted_samples(&converted_input_samples, output_codec_context,
                                   input_frame->nb_samples))
            goto cleanup;

        /**
         * Convert the input samples to the desired output sample format.
         * This requires a temporary storage provided by converted_input_samples.
         */
        int error = swr_convert(swrCxt, converted_input_samples, input_frame->nb_samples,
                                (const uint8_t **) input_frame->extended_data, input_frame->nb_samples);
        if (error < 0){
            LOGError("Could not convert input samples (error%d '%s')\n", error);
            goto cleanup;
        }

        /** Add the converted input samples to the FIFO buffer for later processing. */
        if (add_samples_to_fifo(fifo, converted_input_samples,
                                input_frame->nb_samples))
            goto cleanup;
        ret = 0;
    }
    ret = 0;

    cleanup:
    if (converted_input_samples) {
        av_freep(&converted_input_samples[0]);
        free(converted_input_samples);
    }
    av_frame_free(&input_frame);

    return ret;
}
uint64_t pts = 0;
/** Encode one frame worth of audio to the output file. */
static int encode_audio_frame(AVFrame *frame,
                              AVFormatContext *output_format_context,
                              AVCodecContext *output_codec_context,
                              int *data_present)
{
    /** Packet used for temporary storage. */
    AVPacket output_packet;
    int error;
    av_init_packet(&output_packet);
    output_packet.data = NULL;
    output_packet.size = 0;

    /** Set a timestamp based on the sample rate for the container. */
    if (frame) {
        frame->pts = pts;
        pts += frame->nb_samples;
    }

    /**
     * Encode the audio frame and store it in the temporary packet.
     * The output audio stream encoder is used to do this.
     */
    if ((error = avcodec_encode_audio2(output_codec_context, &output_packet,
                                       frame, data_present)) < 0) {
//        fprintf(stderr, "Could not encode frame (error '%s')\n",
//                get_error_text(error));
        LOGError("Could not encode frame (error%d '%s')\n", error);
        av_packet_unref(&output_packet);
        return error;
    }
    if(frame->pts < 50 * frame->nb_samples){
        for(int i = 0; i < frame->nb_samples; i+=16){
            LOGE("buffer: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x ", frame->data[0][i],
                 frame->data[0][i+1],
                 frame->data[0][i+2],
                 frame->data[0][i+3],
                 frame->data[0][i+4],
                 frame->data[0][i+5],
                 frame->data[0][i+6],
                 frame->data[0][i+7],
                 frame->data[0][i+8],
                 frame->data[0][i+9],
                 frame->data[0][i+10],
                 frame->data[0][i+11],
                 frame->data[0][i+12],
                 frame->data[0][i+13],
                 frame->data[0][i+14],
                 frame->data[0][i+15]);
        }
        LOGE("============================");
    }
    /** Write one audio frame from the temporary packet to the output file. */
    if (*data_present) {
        if ((error = av_write_frame(output_format_context, &output_packet)) < 0) {
//            fprintf(stderr, "Could not write frame (error '%s')\n",
//                    get_error_text(error));
            LOGError("Could not write frame (error%d '%s')\n", error);
            av_packet_unref(&output_packet);
            return error;
        }

        av_packet_unref(&output_packet);
    }

    return 0;
}


/**
 * Load one audio frame from the FIFO buffer, encode and write it to the
 * output file.
 */
static int load_encode_and_write(AVAudioFifo *fifo,
                                 AVFormatContext *output_format_context,
                                 AVCodecContext *output_codec_context)
{
    /** Temporary storage of the output samples of the frame written to the file. */
    AVFrame *output_frame;
    /**
     * Use the maximum number of possible samples per frame.
     * If there is less than the maximum possible frame size in the FIFO
     * buffer use this number. Otherwise, use the maximum possible frame size
     */
    const int frame_size = FFMIN(av_audio_fifo_size(fifo),
                                 output_codec_context->frame_size);
    int data_written;

    /** Initialize temporary storage for one output frame. */
    if (initAvAudioFrame(&output_frame, frame_size))
        return AVERROR_EXIT;
//    for(int i = 0; i < avAudioFrame->channels; i++){
//        memset(avAudioFrame->data[i], 0, avAudioFrame->linesize[0]);
//    }
    /**
     * Read as many samples from the FIFO buffer as required to fill the frame.
     * The samples are stored in the frame temporarily.
     */
    if (av_audio_fifo_read(fifo, (void **)output_frame->data, frame_size) < frame_size) {
//        fprintf(stderr, "Could not read data from FIFO\n");
        LOGE("Could not read data from FIFO\n");
        //av_frame_free(&output_frame);
        return AVERROR_EXIT;
    }

    /** Encode one frame worth of audio samples. */
    if (encode_audio_frame(output_frame, output_format_context,
                           output_codec_context, &data_written)) {
        av_frame_free(&output_frame);
        return AVERROR_EXIT;
    }
    av_frame_free(&output_frame);
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_encodePCMS(JNIEnv *env, jclass type,
                                                           jshortArray shorts_, jint length) {
//    pthread_mutex_lock(&lock);
    jshort *shorts = env->GetShortArrayElements(shorts_, NULL);
    // TODO

    while(1){
        int sendFrame = 0;
        while(av_audio_fifo_size(fifo) < pAudioCodecCxt->frame_size){
            if(read_decode_convert_and_store(fifo, inputFormatCxt, pInputAudioCodecCxt, pAudioCodecCxt, swrCxt, &sendFrame))
                break;
            if(sendFrame)
                break;
        }

        while(av_audio_fifo_size(fifo) >= pAudioCodecCxt->frame_size || (sendFrame && av_audio_fifo_size(fifo) > 0)){
            if(load_encode_and_write(fifo, outFormatCxt, pAudioCodecCxt))
                break;
        }

        if(sendFrame){
            int gotFrame = 0;
            do{
                if(encode_audio_frame(NULL, outFormatCxt, pAudioCodecCxt, &gotFrame))
                    break;
            }while (gotFrame);
            break;
        }
    }

    av_write_trailer(outFormatCxt);
    env->ReleaseShortArrayElements(shorts_, shorts, 0);
//    pthread_mutex_unlock(&lock);
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_encodePCM(JNIEnv *env, jclass type,
                                                           jfloatArray floats_, jint length) {

    jfloat *floats = env->GetFloatArrayElements(floats_, NULL);
//    uint8_t *temp = (uint8_t *) malloc(4);
//    memset(temp, 0, 4);
//    uint8_t *sample = (uint8_t *) malloc(length * 2);
//    memset(bytes, 0, length * 4);
    // TODO
    memset(audioBuffer, 0, audioBufSize);
    //initAvAudioFrame();
    avcodec_fill_audio_frame(avAudioFrame, pAudioCodecCxt->channels, pAudioCodecCxt->sample_fmt,
                             audioBuffer, audioBufSize, 0);
//    memset(inputSample, 0, length);

//    memset(outData[0], 0, pAudioCodecCxt->frame_size);
//    memset(outData[1], 0, pAudioCodecCxt->frame_size);

//    uint8_t ** outData = NULL;
//    uint8_t *inputSample = NULL;

//    inputSample = (uint8_t *) malloc(sizeof(uint8_t));
//    av_samples_alloc(&inputSample, NULL, /*pAudioCodecCxt->channels*/1, pAudioCodecCxt->frame_size, AV_SAMPLE_FMT_FLT, 0);

//    outData = (uint8_t **) calloc(pAudioCodecCxt->channels, sizeof(uint8_t*));
//    av_samples_alloc(outData, NULL, pAudioCodecCxt->channels, pAudioCodecCxt->frame_size, pAudioCodecCxt->sample_fmt, 0);

//    for(int i = 0; i < length / 8 - 4; i++){
////        memcpy(avAudioFrame->data[0]+i*4, &floats[i*8], 4);
////        memcpy(avAudioFrame->data[1]+i*4, &floats[i*8+4], 4);
//        memcpy(convert_samples[0] + i*4, &floats[i*8], 4);
//        memcpy(convert_samples[1] + i*4, &floats[i*8 + 4], 4);
//    }

//    for(int i = 0; i < length; i++){
//        memcpy(inputSample + i*sizeof(jfloat), (uint8_t *) &floats[i], sizeof(jfloat));
//    }

//    int byteCount = av_get_bytes_per_sample((AVSampleFormat)(AV_SAMPLE_FMT_FLT & 0xFF));
//    int nb_inSamples = length;


    int nb_outSamples = av_rescale_rnd(swr_get_delay(swrCxt, pAudioCodecCxt->sample_rate) + length, pAudioCodecCxt->sample_rate, pAudioCodecCxt->sample_rate, AV_ROUND_UP);
    for(int i = 0; i < pAudioCodecCxt->channels; i++){
        memset(outData[i], 0, nb_outSamples);
    }
    int convertSize = swr_convert(swrCxt, outData, nb_outSamples,
                                  (const uint8_t **) &floats, length / pAudioCodecCxt->channels);

//    av_audio_fifo_write(fifo, (void **) outData, convertSize);
//    for(int i = 0; i < length; i++){
//        floats[i] = 0.000000000005f;
//    }
    av_audio_fifo_write(fifo, (void **) outData, convertSize);
//    av_free(&outData[0]);
//    free(outData);
    //av_free((void *) inputSample[0]);
    //free(inputSample);

    while(av_audio_fifo_size(fifo) > avAudioFrame->linesize[0]){
        int readSize = av_audio_fifo_read(fifo, (void **) avAudioFrame->data, avAudioFrame->linesize[0]);
        avAudioFrame->pts += avAudioFrame->nb_samples;
        writeAudioFrame();
    }

//    if(av_audio_fifo_size(fifo) < avAudioFrame->nb_samples)
//        av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo)+(length / 8));
//    else{
//        av_audio_fifo_read(fifo, (void **) avAudioFrame->data, avAudioFrame->nb_samples);
//        writeAudioFrame();
//    }
//    memcpy(avAudioFrame->data[0], outData[0], convertSize);
//    memcpy(avAudioFrame->data[1], outData[1], convertSize);
    //writeAudioFrame();

//    /* encode a single tone sound */
//    avAudioFrame->format = pAudioCodecCxt->sample_fmt;
////    avAudioFrame->pts = av_rescale_q(audiocnt+=avAudioFrame->nb_samples, pAudioCodecCxt->time_base, outFormatCxt->streams[1]->time_base);
//    avAudioFrame->pts += avAudioFrame->nb_samples;
//    avAudioFrame->channels = pAudioCodecCxt->channels;
//    avAudioFrame->channel_layout = pAudioCodecCxt->channel_layout;
//
//    avAudioPacket.data = NULL;
//    avAudioPacket.size = 0;
//    avAudioPacket.pts = 0;
//    float t = 0;
//    float tincr = 2 * M_PI * 440.0 / pAudioCodecCxt->sample_rate;
//    int got_output = 0;
//    uint8_t *samplesL = avAudioFrame->data[0], *samplesR = avAudioFrame->data[1];
//    for (int i = 0; i < 200; i++) {
//        av_init_packet(&avAudioPacket);
//        avAudioPacket.data = NULL; // packet data will be allocated by the encoder
//        avAudioPacket.size = 0;
//        for (int j = 0; j < pAudioCodecCxt->frame_size; j++) {
//            samplesL[j] = (int)(sin(t) * 10000);
//            samplesR[j] = samplesL[j];
//            t += tincr;
//        }
//        /* encode the samples */
//        ret = avcodec_encode_audio2(pAudioCodecCxt, &avAudioPacket, avAudioFrame, &got_output);
//        if (ret < 0) {
//            fprintf(stderr, "Error encoding audio frame\n");
//            exit(1);
//        }
//        if (got_output) {
//            ret = av_interleaved_write_frame(outFormatCxt, &avAudioPacket);
//            if(ret < 0){
//                LOGE("write frame ret: %d", ret);
//            }
//
//            av_packet_unref(&avAudioPacket);
//        }
//    }


//    for(int i = 0; i < length / 8 - 8; i++){
//        for(int j = 0; j < 4; j++){
//            avAudioFrame->data[0][i*4+j] = bytes[i * 8 + j];
//            avAudioFrame->data[1][i*4+j] = bytes[i * 8 + j + 4];
//        }
//
//    }
    //writeAudioFrame();

    env->ReleaseFloatArrayElements(floats_, floats, 0);

    return 0;
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_writeAudioFrame(JNIEnv *env, jclass type) {

    // TODO
    while(av_audio_fifo_size(fifo) > avAudioFrame->nb_samples){
        int readSize = av_audio_fifo_read(fifo, (void **) avAudioFrame->data, avAudioFrame->nb_samples);
        avAudioFrame->pts += avAudioFrame->nb_samples;

        writeAudioFrame();
    }
}
void writeAudioFrame() {
    pthread_mutex_lock(&lock);

    avAudioPacket.data = NULL;
    avAudioPacket.size = 0;
    avAudioPacket.pts = 0;
    av_init_packet(&avAudioPacket);
    int got_audio = 0;
//    int encodeRet = avcodec_encode_audio2(pAudioCodecCxt, &avAudioPacket, avAudioFrame, &got_audio);
//    LOGE("encodeRet: %d, got_audio: %d", encodeRet, got_audio);
//    int sendFrameRet = avcodec_send_frame(pAudioCodecCxt, avAudioFrame);
    int sendFrameRet = avcodec_send_frame(pAudioCodecCxt, avAudioFrame);
    if(sendFrameRet != 0){
        LOGError("send frame ret: %d, %s", sendFrameRet);
        pthread_mutex_unlock(&lock);
        return;
    }
    got_audio = avcodec_receive_packet(pAudioCodecCxt, &avAudioPacket);
    if (got_audio == 0) {
//        if(avAudioPacket.pts < 0)
//            avAudioPacket.pts = 0;
        if(outFormatCxt->streams[0] == NULL){
            LOGE("no stream");
            pthread_mutex_unlock(&lock);
            return;
        }
        avAudioPacket.stream_index = outFormatCxt->streams[0]->index;
//        avAudioPacket.pts = av_rescale_q_rnd(avAudioPacket.pts, pAudioCodecCxt->time_base, outFormatCxt->streams[1]->time_base, AVRounding (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
//        avAudioPacket.duration = av_rescale_q_rnd(avAudioPacket.duration, pAudioCodecCxt->time_base, outFormatCxt->streams[1]->time_base, AVRounding (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
        av_packet_rescale_ts(&avAudioPacket, pAudioCodecCxt->time_base, outFormatCxt->streams[0]->time_base);

//        LOGE("pts: %lld, dura: %d", avAudioPacket.pts, avAudioPacket.duration);
//        AVRational frameRate = {1, 1000};
//        avAudioPacket.pts = av_rescale_q(avAudioPacket.pts, outFormatCxt->streams[1]->time_base,
//                                       frameRate);
//        avAudioPacket.dts = av_rescale_q(avAudioPacket.dts, outFormatCxt->streams[1]->time_base,
//                                         frameRate);
//
////        LOGE("NDK pkt pts: %lld, dts: %lld", avVideoPacket.pts, avVideoPacket.dts);
//        int64_t ptsTime = av_rescale_q(avAudioPacket.dts, outFormatCxt->streams[0]->time_base,
//                                       (AVRational) {1, 1000});
//        int64_t nowTime = av_gettime() - startTime;
////        LOGE("starttime1: %lld", startTime);
////        LOGE("ptstime: %lld, nowtime: %lld", ptsTime, nowTime);
//        if (ptsTime > nowTime) {
//            LOGE("sleeptime: %lld", ptsTime - nowTime);
//            av_usleep(100);
//        }

        if(avAudioPacket.pts == avAudioPacket.duration){
            audioStartTime = avAudioPacket.dts;
        }
        int64_t pts = avAudioPacket.dts, now = av_gettime() - audioStartTime;
        if(pts > now){
            LOGE("pts: %lld, now: %lld", pts, now);
            av_usleep(pts - now);
        }
//LOGE("audio pkt pts:%lld", avAudioPacket.pts);
        ret = av_write_frame(outFormatCxt, &avAudioPacket);
        if(ret < 0){
            LOGError("write audio frame ret: %d, %s", ret);
        }


        av_packet_unref(&avAudioPacket);
    }

    pthread_mutex_unlock(&lock);

}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_flush(JNIEnv *env, jclass type) {

    // TODO
    int ret, gotFrame;
    AVPacket avPacket1;
    if (!(outFormatCxt->streams[0]->codec->codec->capabilities & CODEC_CAP_DELAY)) {
        return 0;
    }
    while (1) {
        avPacket1.data = NULL;
        avPacket1.size = 0;
        av_init_packet(&avPacket1);
        ret = avcodec_send_frame(pVideoCodecCxt, avVideoFrame);
        gotFrame = avcodec_receive_packet(pVideoCodecCxt, &avPacket1);
        if (ret < 0)
            break;
        if (gotFrame != 0) {
            ret = 0;
            break;
        }
        LOGI("Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n", avPacket1.size);
        AVRational time_base = outFormatCxt->streams[0]->time_base;
        AVRational r_framerate1 = {60, 2};
        AVRational base = {1, AV_TIME_BASE};
        int64_t calc_duration = (double) (AV_TIME_BASE) * (1 / av_q2d(r_framerate1));
        avPacket1.pts = av_rescale_q(framecnt * calc_duration, base, time_base);
        avPacket1.dts = avPacket1.pts;
        avPacket1.duration = av_rescale_q(calc_duration, base, time_base);
        avPacket1.pos = -1;
        framecnt++;
        outFormatCxt->duration = avPacket1.duration * framecnt;

        ret = av_interleaved_write_frame(outFormatCxt, &avPacket1);
        if (ret < 0)
            break;
    }

    return 0;
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_close(JNIEnv *env, jclass type) {

    // TODO
//    if(avStream){
//        avcodec_close(pVideoCodecCxt);
//    }
//    avio_close(outFormatCxt->pb);
//    avformat_free_context(outFormatCxt);
    av_free(&outData[0]);
    free(outData);
    av_frame_free(&avVideoFrame);
    av_write_trailer(outFormatCxt);
    end(NULL, outFormatCxt);
    return 0;
}

JNIEXPORT void JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_initStartTime(JNIEnv *, jclass) {
    startTime = av_gettime();
    audioStartTime = av_gettime();
    framecnt = 2;
    audiocnt = 0;

}
}
