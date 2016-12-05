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
#include "NDKImpl.h"

extern "C" {
#include "libavutil/log.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#include "libavutil/imgutils.h"
#include "libavutil/time.h"
#include "libavformat/avformat.h"
#include "libavfilter/avfilter.h"
#include "libavdevice/avdevice.h"
#include "libpostproc/postprocess.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"

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

int ret = 0;
AVFormatContext *outFormatCxt = NULL;
AVStream *avStream = NULL;
AVCodecContext *pCodecCxt = NULL;
//use AVCodecParameters instead of AVCodecContext
AVCodecParameters *pCodecParam = NULL;
AVCodec *avCodec = NULL;
AVPacket avPacket;
AVFrame *avFrame = NULL;

int framecnt = 0;
int yuvWidth = 0;
int yuvHeight = 0;
int yLength = 0;
int uvLenght = 0;
int64_t startTime = 0;

jint end(AVFormatContext **inputFormatContext, AVFormatContext *outputFormatContext) {
    if (*inputFormatContext != NULL)
        avformat_close_input(inputFormatContext);
    if (outputFormatContext && !(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
        avio_close(outputFormatContext->pb);
    }
    avformat_free_context(outputFormatContext);

    if(pCodecCxt != NULL)
        avcodec_free_context(&pCodecCxt);
    if(pCodecParam != NULL)
        avcodec_parameters_free(&pCodecParam);
    if(avFrame != NULL)
        av_frame_free(&avFrame);
    if (ret < 0 && ret == AVERROR_EOF) {
        LOGE("unknow error");
        return -1;
    }
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
            break;
        }
        av_packet_unref(&packet);
    }

    av_write_trailer(outputFormatContext);

    env->ReleaseStringUTFChars(input_, input);
    env->ReleaseStringUTFChars(output_, output);
    int rst = end(&inputFormatContext, outputFormatContext);
    return rst;
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

    av_log_set_callback(custom_log);

    av_register_all();

    avformat_network_init();

    avformat_alloc_output_context2(&outFormatCxt, NULL, "flv", outputUrl);

    if (!(avCodec = avcodec_find_encoder(AV_CODEC_ID_H264))) {
        LOGE("init encoder failed");
        ret = -1;
        return end(NULL, outFormatCxt);
    }

    {
        if (!(pCodecCxt = avcodec_alloc_context3(avCodec))) {
            LOGE("init avCodeContext failed");
            ret = -1;
            return end(NULL, outFormatCxt);
        }
        pCodecCxt->pix_fmt = AV_PIX_FMT_YUV420P;
        pCodecCxt->width = width;
        pCodecCxt->height = height;
        pCodecCxt->time_base.num = 1;
        pCodecCxt->time_base.den = 30;
        pCodecCxt->bit_rate = 800 * 1000; //传输速率 rate/kbps
        pCodecCxt->gop_size = 12; //gop = fps/N ?
        pCodecCxt->max_b_frames = 24; // fps?
        pCodecCxt->qmin = 10; // 1-51, 10-30 is better
        pCodecCxt->qmax = 30; // 1-51, 10-30 is better
        AVDictionary *param = 0;
        av_dict_set(&param, "preset", "ultrafast", 0);
        av_dict_set(&param, "tune", "zerolatency", 0);

        if ((ret = avcodec_open2(pCodecCxt, avCodec, &param)) < 0) {
            LOGE("open encoder failed");
            ret = -1;
            return end(NULL, outFormatCxt);
        }

        if (!(avStream = avformat_new_stream(outFormatCxt, avCodec))) {
            LOGE("create avStream failed");
            ret = -1;
            return end(NULL, outFormatCxt);
        }
        avStream->time_base = pCodecCxt->time_base;
        //avStream->codec = pCodecCxt;
    }

    //test init AVCodecParamters
    {
        if(!(pCodecParam = avcodec_parameters_alloc())){
            LOGE("init pCodecParam failed");
            ret = -1;
            return end(NULL, outFormatCxt);
        }
        avcodec_parameters_from_context(pCodecParam, pCodecCxt);
        avStream->codecpar = pCodecParam;
    }

    if((ret = avio_open(&outFormatCxt->pb, outputUrl, AVIO_FLAG_WRITE)) < 0){
        LOGE("failed to open outputUrl: %s", outputUrl);
        return end(NULL, outFormatCxt);
    }

    if (outFormatCxt->oformat->flags & AVFMT_GLOBALHEADER)
        pCodecCxt->flags |= CODEC_FLAG_GLOBAL_HEADER;

    avformat_write_header(outFormatCxt, NULL);
    startTime = av_gettime();

    env->ReleaseStringUTFChars(outputUrl_, outputUrl);

    return 0;
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_encodeYUV(JNIEnv *env, jclass type,
                                                           jbyteArray yuvData_) {
    jbyte *yuvData = env->GetByteArrayElements(yuvData_, NULL);

    int encGotFrame = 0;
    int i = 0;
    // TODO

    avFrame = av_frame_alloc();
    uint8_t *frameBuffer = (uint8_t*) av_malloc(av_image_get_buffer_size(pCodecCxt->pix_fmt, yuvWidth, yuvHeight, 1));
    av_image_fill_arrays(avFrame->data, avFrame->linesize, frameBuffer, pCodecCxt->pix_fmt, yuvWidth, yuvHeight, 1);

    //convert android camera data from NV21 to yuv420p
    memcpy(avFrame->data[0], yuvData, yLength);
    for(;i < uvLenght; i++){
        *(avFrame->data[2] + i) = *(yuvData + yLength + i * 2);
        *(avFrame->data[1] + i) = *(yuvData + yLength + i * 2 + 1);
    }
    avFrame->format = AV_PIX_FMT_YUV420P;
    avFrame->width = yuvWidth;
    avFrame->height = yuvHeight;

    LOGE("NDK %d", avFrame->flags);

    avPacket.data = NULL;
    avPacket.size = 0;
    av_init_packet(&avPacket);
//    ret = avcodec_encode_video2()
    ret = avcodec_send_frame(pCodecCxt, avFrame);
    encGotFrame = avcodec_receive_packet(pCodecCxt, &avPacket);
    LOGE("NDK send ret %d", ret);
    LOGE("NDK receive ret %d", encGotFrame);
    av_frame_free(&avFrame);

    if(encGotFrame == 0){
        framecnt++;
        avPacket.stream_index = avStream->index;

        AVRational timeBase = outFormatCxt->streams[0]->time_base;
        AVRational frameRate = {60, 2};
        AVRational base = {1, AV_TIME_BASE};
        int64_t caclDuration = (double)(AV_TIME_BASE)* (1 / av_q2d(frameRate));
        avPacket.pts = av_rescale_q(framecnt * caclDuration, base, timeBase);
        avPacket.dts = avPacket.pts;
        avPacket.duration = av_rescale_q(caclDuration, base, timeBase);
        avPacket.pos = -1;

        int64_t ptsTime = av_rescale_q(avPacket.dts, timeBase, base);
        int64_t nowTime = av_gettime() - startTime;
        if(ptsTime > nowTime){
            av_usleep(ptsTime - nowTime);
        }

        ret = av_interleaved_write_frame(outFormatCxt, &avPacket);
        av_packet_unref(&avPacket);
        LOGE("NDK ret %d", ret);
    }

    env->ReleaseByteArrayElements(yuvData_, yuvData, 0);
    return ret;
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_flush(JNIEnv *env, jclass type) {

    // TODO

}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_close(JNIEnv *env, jclass type) {

    // TODO

}
}
