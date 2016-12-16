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
int ret = 0;
int framecnt = 2;
int yuvWidth = 0;
int yuvHeight = 0;
int yLength = 0;
int uvLenght = 0;
int64_t startTime = 0;

int audiocnt = 0;

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
    avcodec_register_all();

    avformat_network_init();

    avformat_alloc_output_context2(&outFormatCxt, NULL, "flv", outputUrl);
    if (!(avVideoCodec = avcodec_find_encoder(AV_CODEC_ID_H264))) {
        LOGE("init h264 encoder failed");
        ret = -1;
        return end(NULL, outFormatCxt);
    }


    if (!(avAudioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC))) {
//    if (!(avAudioCodec = avcodec_find_encoder_by_name("libfdk_aac"))) {
        LOGE("init aac encoder failed");
        ret = -1;
        return end(NULL, outFormatCxt);
    }

    if (initVideoCodecContext() < 0)
        return end(NULL, outFormatCxt);

    if (initAudioCodecContext() < 0) {
        return end(NULL, outFormatCxt);
    }

    AVDictionary *param = NULL;
    av_dict_set(&param, "preset", "ultrafast"/*"slow"*/, 0);
    av_dict_set(&param, "tune", "zerolatency", 0);
    if ((ret = avcodec_open2(pVideoCodecCxt, avVideoCodec, &param)) < 0) {
        LOGE("open encoder failed");
        return end(NULL, outFormatCxt);
    }

    if ((ret = avcodec_open2(pAudioCodecCxt, avAudioCodec, NULL)) < 0) {
        LOGE("open encoder failed %s", av_err2str(ret));
        return end(NULL, outFormatCxt);
    }

    AVStream *avStream = initAvStream();
    if (!avStream) {
        return end(NULL, outFormatCxt);
    }

    AVStream *audioStream = initAudioStream();
    if (!audioStream) {
        return end(NULL, outFormatCxt);
    }

    if ((ret = avio_open(&outFormatCxt->pb, outputUrl, AVIO_FLAG_WRITE)) < 0) {
        LOGE("failed to open outputUrl: %s", outputUrl);
        return end(NULL, outFormatCxt);
    }

    if (outFormatCxt->oformat->flags & AVFMT_GLOBALHEADER)
        pVideoCodecCxt->flags |= CODEC_FLAG_GLOBAL_HEADER;

    if (outFormatCxt->oformat->flags & AVFMT_GLOBALHEADER)
        pAudioCodecCxt->flags |= CODEC_FLAG_GLOBAL_HEADER;

    avformat_write_header(outFormatCxt, NULL);
    //avStream->time_base.den = 900;
//    startTime = av_gettime();

    avVideoFrame = av_frame_alloc();
    frameBufSize = av_image_get_buffer_size(pVideoCodecCxt->pix_fmt, yuvWidth, yuvHeight, 1);
    frameBuffer = (uint8_t *) av_malloc(frameBufSize);

    avAudioFrame = av_frame_alloc();
    audioBufSize = av_samples_get_buffer_size(NULL, pAudioCodecCxt->channels,
                                              pAudioCodecCxt->frame_size,
                                              pAudioCodecCxt->sample_fmt, 1);
    audioBuffer = (uint8_t *) av_malloc(audioBufSize);

    //initSwrContext();

    env->ReleaseStringUTFChars(outputUrl_, outputUrl);
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
    pVideoCodecCxt->bit_rate = 800 * 1000; //传输速率 rate/kbps
    pVideoCodecCxt->gop_size = 12; //gop = fps/N ?
    pVideoCodecCxt->max_b_frames = 23; // fps?
    pVideoCodecCxt->qmin = 10; // 1-51, 10-30 is better
    pVideoCodecCxt->qmax = 30; // 1-51, 10-30 is better
    return 0;
}

int initAudioCodecContext() {
    if (!(pAudioCodecCxt = avcodec_alloc_context3(avAudioCodec))) {
        LOGE("init avAudioContext failed");
        return -1;
    }

    pAudioCodecCxt->sample_fmt = avAudioCodec->sample_fmts[0];
    pAudioCodecCxt->sample_rate = 44100;
    pAudioCodecCxt->channel_layout = AV_CH_LAYOUT_STEREO;
    pAudioCodecCxt->channels = av_get_channel_layout_nb_channels(pAudioCodecCxt->channel_layout);
    pAudioCodecCxt->bit_rate = 64 * 1000;
    pAudioCodecCxt->time_base = {1, pAudioCodecCxt->sample_rate};
    return 0;
//    pAudioCodecCxt->codec_id = outFormatCxt->oformat->audio_codec;
//    pAudioCodecCxt->codec_type = AVMEDIA_TYPE_AUDIO;

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
    if (!(avStream = avformat_new_stream(outFormatCxt, avAudioCodec))) {
        LOGE("create audioStream failed");
        ret = -1;
        return NULL;
    }
    avcodec_parameters_from_context(avStream->codecpar, pAudioCodecCxt);
    return avStream;
}

//void initSwrContext(){
//    AVFifoBuffer **fifo = NULL;
//    swrCxt = swr_alloc();
//
//#if LIBSWRESAMPLE_VERSION_MINOR >= 17
//    av_opt_set_int(swrCxt, "inputChannel", pAudioCodecCxt->channels, 0);
//    av_opt_set_int(swrCxt, "outputChannel", pAudioCodecCxt->channels, 0);
//    av_opt_set_int(swrCxt, "inputSampleRate", pAudioCodecCxt->sample_rate, 0);
//    av_opt_set_int(swrCxt, "outputSampleRate", pAudioCodecCxt->sample_rate, 0);
//    av_opt_set_sample_fmt(swrCxt, "inputFmt", AV_SAMPLE_FMT_S16P, 0);
//    av_opt_set_sample_fmt(swrCxt, "outputFmt", pAudioCodecCxt->sample_fmt, 0);
//#else
//    swrCxt = swr_alloc_set_opts(swrCxt,
//                                pAudioCodecCxt->channel_layout,
//                                pAudioCodecCxt->sample_fmt,
//                                pAudioCodecCxt->sample_rate,
//                                pAudioCodecCxt->channel_layout,
//                                AV_SAMPLE_FMT_S16P,
//                                pAudioCodecCxt->sample_rate,
//                                0, NULL);
//#endif
//    swr_init(swrCxt);
//    for(int i = 0; i < pAudioCodecCxt->channels; i++){
//        fifo[i] = av_fifo_alloc(20*1000);
//    }
//    int ret = 0;
//    int linesize = 0;
//    int nbsample = 0;
//    ret = av_samples_alloc_array_and_samples(&outData, &linesize, pAudioCodecCxt->channels, nbsample, pAudioCodecCxt->sample_fmt, 0);
//    nbsample = av_rescale_rnd(swr_get_delay(swrCxt, pAudioCodecCxt->sample_rate), pAudioCodecCxt->sample_rate, pAudioCodecCxt->sample_rate, AV_ROUND_UP);
//
//}

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

        ret = av_interleaved_write_frame(outFormatCxt, &avVideoPacket);
        av_packet_unref(&avVideoPacket);
        //av_frame_free(&avVideoFrame);
    }
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_encodePCM(JNIEnv *env, jclass type,
                                                           jbyteArray bytes_, jint length) {
    jbyte *bytes = env->GetByteArrayElements(bytes_, NULL);
//    uint8_t *temp = (uint8_t *) malloc(4);
//    memset(temp, 0, 4);
//    uint8_t *sample = (uint8_t *) malloc(length * 2);
//    memset(bytes, 0, length * 4);
    // TODO
    memset(audioBuffer, 0, audioBufSize);
    avcodec_fill_audio_frame(avAudioFrame, pAudioCodecCxt->channels, pAudioCodecCxt->sample_fmt,
                             audioBuffer, audioBufSize, 1);
    avAudioFrame->data[0] = audioBuffer;
    avAudioFrame->data[1] = audioBuffer;
//    for(int i = 0; i < length; i++){
//        memcpy(temp, &floats[i], 4);
//        for(int j = 3; j >=0; j--){
//            *(bytes+i*4 + (3 - j)) = temp[j];
//        }
//    }
    memcpy(avAudioFrame->data[0], bytes, audioBufSize);
    memcpy(avAudioFrame->data[1], bytes, audioBufSize);
    writeAudioFrame();

    env->ReleaseByteArrayElements(bytes_, bytes, 0);
    return 0;
}

void writeAudioFrame() {
    avAudioFrame->nb_samples = pAudioCodecCxt->frame_size;
    avAudioFrame->format = pAudioCodecCxt->sample_fmt;
    avAudioFrame->pts = ++audiocnt;

    avAudioPacket.data = NULL;
    avAudioPacket.size = 0;
    av_init_packet(&avAudioPacket);
    int got_audio = 0;
    avcodec_encode_audio3(pAudioCodecCxt, &avAudioPacket, avAudioFrame, &got_audio);
//    avcodec_send_frame(pAudioCodecCxt, avAudioFrame);
//    got_audio = avcodec_receive_packet(pAudioCodecCxt, &avAudioPacket);

    if (got_audio == 0) {
        avAudioPacket.stream_index = outFormatCxt->streams[1]->index;
        av_interleaved_write_frame(outFormatCxt, &avAudioPacket);
        av_packet_unref(&avAudioPacket);
    }

}

int avcodec_encode_audio3(AVCodecContext *avctx,
                          AVPacket *avpkt,
                          const AVFrame *frame,
                          int *got_packet_ptr) {
    AVFrame *extended_frame = NULL;
    AVFrame *padded_frame = NULL;
    int ret;
    AVPacket user_pkt = *avpkt;
    int needs_realloc = !user_pkt.data;

    *got_packet_ptr = 0;

    if (!avctx->codec->encode2) {
        av_log(avctx, AV_LOG_ERROR, "This encoder requires using the avcodec_send_frame() API.\n");
        return AVERROR(ENOSYS);
    }

    if (!(avctx->codec->capabilities & AV_CODEC_CAP_DELAY) && !frame) {
        av_packet_unref(avpkt);
        av_init_packet(avpkt);
        return 0;
    }

/* ensure that extended_data is properly set */
    if (frame && !frame->extended_data) {
        if (av_sample_fmt_is_planar(avctx->sample_fmt) &&
            avctx->channels > AV_NUM_DATA_POINTERS) {
            av_log(avctx, AV_LOG_ERROR, "Encoding to a planar sample format, "
                           "with more than %d channels, but extended_data is not set.\n",
                   AV_NUM_DATA_POINTERS);
            return AVERROR(EINVAL);
        }
        av_log(avctx, AV_LOG_WARNING, "extended_data is not set.\n");

        extended_frame = av_frame_alloc();
        if (!extended_frame)
            return AVERROR(ENOMEM);

        memcpy(extended_frame, frame, sizeof(AVFrame));
        extended_frame->extended_data = extended_frame->data;
        frame = extended_frame;
    }

/* extract audio service type metadata */
    if (frame) {
        AVFrameSideData *sd = av_frame_get_side_data(frame, AV_FRAME_DATA_AUDIO_SERVICE_TYPE);
        if (sd && sd->size >= sizeof(enum AVAudioServiceType))
            avctx->audio_service_type = *(enum AVAudioServiceType *) sd->data;
    }

/* check for valid frame size */
    if (frame) {
        if (avctx->codec->capabilities & AV_CODEC_CAP_SMALL_LAST_FRAME) {
            if (frame->nb_samples > avctx->frame_size) {
                av_log(avctx, AV_LOG_ERROR,
                       "more samples than frame size (avcodec_encode_audio2)\n");
                ret = AVERROR(EINVAL);
                goto end;
            }
        } else if (!(avctx->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)) {
            if (frame->nb_samples < avctx->frame_size
                /*&& !avctx->internal->last_audio_frame*/) {
                ret = pad_last_frame(avctx, &padded_frame, frame);
                if (ret < 0)
                    goto end;

                frame = padded_frame;
                //avctx->internal->last_audio_frame = 1;
            }

            if (frame->nb_samples != avctx->frame_size) {
                av_log(avctx, AV_LOG_ERROR,
                       "nb_samples (%d) != frame_size (%d) (avcodec_encode_audio2)\n",
                       frame->nb_samples, avctx->frame_size);
                ret = AVERROR(EINVAL);
                goto end;
            }
        }
    }

    av_assert0(avctx->codec->encode2);
//    ret = aac_encode_frame(avctx, avpkt, frame, got_packet_ptr);
    ret = avctx->codec->encode2(avctx, avpkt, frame, got_packet_ptr);
    if (!ret) {
        if (*got_packet_ptr) {
            if (!(avctx->codec->capabilities & AV_CODEC_CAP_DELAY)) {
                if (avpkt->pts == AV_NOPTS_VALUE)
                    avpkt->pts = frame->pts;
                if (!avpkt->duration)
                    avpkt->duration = ff_samples_to_time_base(avctx,
                                                              frame->nb_samples);
            }
            avpkt->dts = avpkt->pts;
        } else {
            avpkt->size = 0;
        }
    }
    if (avpkt->data /*&& avpkt->data == avctx->internal->byte_buffer*/) {
        needs_realloc = 0;
        if (user_pkt.data) {
            if (user_pkt.size >= avpkt->size) {
                memcpy(user_pkt.data, avpkt->data, avpkt->size);
            } else {
                av_log(avctx, AV_LOG_ERROR, "Provided packet is too small, needs to be %d\n",
                       avpkt->size);
                avpkt->size = user_pkt.size;
                ret = -1;
            }
            avpkt->buf = user_pkt.buf;
            avpkt->data = user_pkt.data;
        } else {
            if (av_dup_packet(avpkt) < 0) {
                ret = AVERROR(ENOMEM);
            }
        }
    }

    if (!ret) {
        if (needs_realloc && avpkt->data) {
            ret = av_buffer_realloc(&avpkt->buf, avpkt->size + AV_INPUT_BUFFER_PADDING_SIZE);
            if (ret >= 0)
                avpkt->data = avpkt->buf->data;
        }

        avctx->frame_number++;
    }

    if (ret < 0 || !*got_packet_ptr) {
        av_packet_unref(avpkt);
        av_init_packet(avpkt);
        goto end;
    }

/* NOTE: if we add any audio encoders which output non-keyframe packets,
 *       this needs to be moved to the encoders, but for now we can do it
 *       here to simplify things */
    avpkt->flags |= AV_PKT_FLAG_KEY;

    end:
    av_frame_free(&padded_frame);
    av_free(extended_frame);

#if FF_API_AUDIOENC_DELAY
    avctx->delay = avctx->initial_padding;
#endif

    return ret;
}



static int pad_last_frame(AVCodecContext *s, AVFrame **dst, const AVFrame *src)
{
    AVFrame *frame = NULL;
    int ret;

    if (!(frame = av_frame_alloc()))
        return AVERROR(ENOMEM);

    frame->format         = src->format;
    frame->channel_layout = src->channel_layout;
    av_frame_set_channels(frame, av_frame_get_channels(src));
    frame->nb_samples     = s->frame_size;
    ret = av_frame_get_buffer(frame, 32);
    if (ret < 0)
        goto fail;

    ret = av_frame_copy_props(frame, src);
    if (ret < 0)
        goto fail;

    if ((ret = av_samples_copy(frame->extended_data, src->extended_data, 0, 0,
                               src->nb_samples, s->channels, s->sample_fmt)) < 0)
        goto fail;
    if ((ret = av_samples_set_silence(frame->extended_data, src->nb_samples,
                                      frame->nb_samples - src->nb_samples,
                                      s->channels, s->sample_fmt)) < 0)
        goto fail;

    *dst = frame;

    return 0;

    fail:
    av_frame_free(&frame);
    return ret;
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
    av_frame_free(&avVideoFrame);
    av_write_trailer(outFormatCxt);
    end(NULL, outFormatCxt);
    return 0;
}

JNIEXPORT void JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_initStartTime(JNIEnv *, jclass) {
    startTime = av_gettime();
//    framecnt = 0;
    LOGE("starttime: %lld", startTime);
}
}
