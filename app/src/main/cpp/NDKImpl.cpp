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
#include "PrivateUtils.h"

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


//AVFormatContext *outFormatCxt = NULL;
//AVStream *avStream = NULL;
//AVCodecContext *pCodecCxt = NULL;
////use AVCodecParameters instead of AVCodecContext
//AVCodecParameters *pCodecParam = NULL;
//AVCodec *avCodec = NULL;
//AVPacket avPacket;
//AVFrame *avFrame = NULL;
int ret = 0;
int framecnt = 0;
int yuvWidth = 0;
int yuvHeight = 0;
int yLength = 0;
int uvLenght = 0;
int64_t startTime = 0;

jint end(AVFormatContext **inputFormatContext, AVFormatContext *outputFormatContext) {
    if (inputFormatContext != NULL)
        avformat_close_input(inputFormatContext);
    if (outputFormatContext && !(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
        avio_close(outputFormatContext->pb);
    }
    avformat_free_context(outputFormatContext);

    if (pCodecCxt != NULL) {
        avcodec_close(pCodecCxt);
        avcodec_free_context(&pCodecCxt);
    }
    if (avFrame != NULL)
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

    if (initCodecContext() < 0)
        return end(NULL, outFormatCxt);

    AVDictionary *param = NULL;
    av_dict_set(&param, "preset", "slow", 0);
    av_dict_set(&param, "tune", "zerolatency", 0);
    if ((ret = avcodec_open2(pCodecCxt, avCodec, &param)) < 0) {
        LOGE("open encoder failed");
        return end(NULL, outFormatCxt);
    }

    AVStream * avStream = initAvStream();
    if(!avStream){
        return end(NULL, outFormatCxt);
    }

    if ((ret = avio_open(&outFormatCxt->pb, outputUrl, AVIO_FLAG_WRITE)) < 0) {
        LOGE("failed to open outputUrl: %s", outputUrl);
        return end(NULL, outFormatCxt);
    }

    if (outFormatCxt->oformat->flags & AVFMT_GLOBALHEADER)
        pCodecCxt->flags |= CODEC_FLAG_GLOBAL_HEADER;

    avformat_write_header(outFormatCxt, NULL);
    avStream->time_base.den = 900;
//    startTime = av_gettime();

    avFrame = av_frame_alloc();
    bufferSize = av_image_get_buffer_size(pCodecCxt->pix_fmt, yuvWidth, yuvHeight, 1);
    frameBuffer = (uint8_t *) av_malloc(bufferSize);

    env->ReleaseStringUTFChars(outputUrl_, outputUrl);
    return 0;
}

int initCodecContext() {
    if (!(pCodecCxt = avcodec_alloc_context3(avCodec))) {
        LOGE("init avCodeContext failed");
        return -1;
    }
    pCodecCxt->pix_fmt = AV_PIX_FMT_YUV420P;
    pCodecCxt->width = yuvWidth;
    pCodecCxt->height = yuvHeight;
    pCodecCxt->time_base.num = 1;
    pCodecCxt->time_base.den = 25;
    pCodecCxt->bit_rate = 500 * 1000; //传输速率 rate/kbps
    pCodecCxt->gop_size = 12; //gop = fps/N ?
    pCodecCxt->max_b_frames = 24; // fps?
    pCodecCxt->qmin = 10; // 1-51, 10-30 is better
    pCodecCxt->qmax = 30; // 1-51, 10-30 is better
    return 0;
}

AVStream* initAvStream() {
    AVStream *avStream = NULL;
    if (!(avStream = avformat_new_stream(outFormatCxt, avCodec))) {
        LOGE("create avStream failed");
        ret = -1;
        return NULL;
    }
    avcodec_parameters_from_context(avStream->codecpar, pCodecCxt);
    return avStream;
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_encodeYUV(JNIEnv *env, jclass type,
                                                           jbyteArray yuvData_) {
    jbyte *yuvData = env->GetByteArrayElements(yuvData_, NULL);

    // TODO

    if (pCodecCxt == NULL) {
        ret = -1;
        return end(NULL, outFormatCxt);
    }

    av_image_fill_arrays(avFrame->data, avFrame->linesize, frameBuffer, pCodecCxt->pix_fmt,
                         yuvWidth, yuvHeight, 1);

    //convert android camera data from NV21 to yuv420p
    memcpy(avFrame->data[0], yuvData, yLength);
    for (int i = 0; i < uvLenght; i++) {
        *(avFrame->data[2] + i) = *(yuvData + yLength + i * 2);
        *(avFrame->data[1] + i) = *(yuvData + yLength + i * 2 + 1);
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


    if (pCodecCxt == NULL) {
        ret = -1;
        return end(NULL, outFormatCxt);
    }
    memset(frameBuffer, 0, bufferSize);
    av_image_fill_arrays(avFrame->data, avFrame->linesize, frameBuffer, pCodecCxt->pix_fmt,
                         yuvWidth, yuvHeight, 1);

    analyzeYUVData(yData, uData, vData, rowStride, pixelStride);
    writeFrame();
    env->ReleaseByteArrayElements(yData_, yData, 0);
    env->ReleaseByteArrayElements(uData_, uData, 0);
    env->ReleaseByteArrayElements(vData_, vData, 0);
    return ret;
}

void analyzeYUVData(jbyte *y, jbyte *u, jbyte *v, jint rowStride, jint pixelStride){
    int offset = 0;
    int length = yuvWidth;
    for (int row = 0; row < yuvHeight; row++) {
        memcpy((avFrame->data[0] + length), y, yuvWidth);
        length += yuvWidth;
        y += rowStride;
        if(row < yuvHeight >> 1){
            for (int col = 0; col < yuvWidth >> 1; col++) {
                *(avFrame->data[1] + offset) = *(u + col * pixelStride);
                *(avFrame->data[2] + offset++) = *(v + col * pixelStride);
            }
            u += rowStride;
            v += rowStride;
        }
    }
    framecnt++;
    avFrame->pts = framecnt;
}

void writeFrame() {
    avFrame->format = AV_PIX_FMT_YUV420P;
    avFrame->width = yuvWidth;
    avFrame->height = yuvHeight;
//    avFrame->pts = framecnt++ * 1000 / 25;

    avPacket.data = NULL;
    avPacket.size = 0;
    av_init_packet(&avPacket);
    int g = 0;
    int encGotFrame = avcodec_encode_video2(pCodecCxt, &avPacket, avFrame, &g);
//    ret = avcodec_send_frame(pCodecCxt, avFrame);
//    int encGotFrame = avcodec_receive_packet(pCodecCxt, &avPacket);

    if (g) {
//        framecnt++;
//        avPacket.stream_index = outFormatCxt->streams[0]->index;
//
//        AVRational timeBase = outFormatCxt->streams[0]->time_base;
        AVRational frameRate = {25, 1};
////        AVRational frameRate = outFormatCxt->streams[0]->r_frame_rate;
//        AVRational base = {1, AV_TIME_BASE};
//        int64_t caclDuration = (double) (AV_TIME_BASE)  / av_q2d(frameRate);
////        avPacket.pts = av_rescale_q_rnd(framecnt * caclDuration, base, timeBase, AV_ROUND_NEAR_INF);
//        avPacket.pts = av_rescale_q(framecnt * caclDuration, base, timeBase);
//        avPacket.dts = avPacket.pts;
//        avPacket.duration = av_rescale_q(caclDuration, base, timeBase);
//        avPacket.pos = -1;
        avPacket.pts = av_rescale(avPacket.pts, outFormatCxt->streams[0]->time_base.den, 18);
        avPacket.dts = av_rescale(avPacket.dts, outFormatCxt->streams[0]->time_base.den, 18);
        avPacket.duration = (AV_TIME_BASE)  / av_q2d(frameRate) / 1000;
        avPacket.pos = -1;

        int64_t ptsTime = av_rescale_q(avPacket.dts, outFormatCxt->streams[0]->time_base, (AVRational){1, 25});
        int64_t nowTime = av_gettime() - startTime;
//        LOGE("starttime1: %lld", startTime);
//        LOGE("ptstime: %lld, nowtime: %lld", ptsTime, nowTime);
        if (ptsTime > nowTime) {
            LOGE("sleeptime: %lld", ptsTime-nowTime);
            av_usleep(ptsTime - nowTime);
        }

        ret = av_interleaved_write_frame(outFormatCxt, &avPacket);
        av_packet_unref(&avPacket);
        //av_frame_free(&avFrame);
    }
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
        ret = avcodec_send_frame(pCodecCxt, avFrame);
        gotFrame = avcodec_receive_packet(pCodecCxt, &avPacket1);
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
//        avcodec_close(pCodecCxt);
//    }
//    avio_close(outFormatCxt->pb);
//    avformat_free_context(outFormatCxt);
    av_frame_free(&avFrame);
    av_write_trailer(outFormatCxt);
    end(NULL, outFormatCxt);
    return 0;
}

JNIEXPORT void JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_initStartTime(JNIEnv , jclass){
    startTime = av_gettime();
    LOGE("starttime: %lld", startTime);
}
}
