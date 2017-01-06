//
// Created by Mikiller on 2016/11/25.
//
#ifndef TAG
#define TAG "NDKImpl.cpp"
#endif

#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include "NDKImpl.h"
#include "Utils.h"
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

pthread_mutex_t lock;
pthread_mutex_t datalock;

int ret = 0;
int framecnt = 2;
int yuvWidth = 0;
int yuvHeight = 0;
int yLength = 0;
int uvLenght = 0;
int64_t startTime = 0;
int64_t audioStartTime = 0;

int64_t videoPts = 0, audioPts = 0;

int audiocnt = 0;

jint end(AVFormatContext **inputFormatContext, AVFormatContext *outputFormatContext) {
    if (inputFormatContext != NULL)
        avformat_close_input(inputFormatContext);
    if (outputFormatContext) {
//        if(outputFormatContext->oformat && !(outputFormatContext->oformat->flags & AVFMT_NOFILE)){
//            avio_close(outputFormatContext->pb);
//        }
        avformat_free_context(outputFormatContext);
        LOGE("free outputFormat");
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
    pthread_mutex_destroy(&datalock);
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

static void registFFMpeg() {
    av_log_set_callback(custom_log);

    av_register_all();
    avcodec_register_all();

    avformat_network_init();
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_initFFMpeg(JNIEnv *env, jclass type,
                                                            jstring outputUrl_, jint width,
                                                            jint height,
                                                            jint channels, jint sampleFmt,
                                                            jint sampleRate) {
    const char *outputUrl = env->GetStringUTFChars(outputUrl_, 0);

    // TODO
    yuvWidth = width, yuvHeight = height;
    yLength = width * height;
    uvLenght = yLength / 4;
    inputChannels = channels;
    switch (sampleFmt) {
        case 4:
            inputSampleFmt = AV_SAMPLE_FMT_FLT;
            break;
        default:
            inputSampleFmt = AV_SAMPLE_FMT_S16;
    }
    inputSampleRate = sampleRate;
    registFFMpeg();

    avformat_alloc_output_context2(&outFormatCxt, NULL, "flv", outputUrl);
    if ((ret = avio_open(&outFormatCxt->pb, outputUrl, AVIO_FLAG_WRITE)) < 0) {
        LOGE("failed to open outputUrl: %s", outputUrl);
        return end(NULL, outFormatCxt);
    }
//获取视频编码器x264
    if (!(avVideoCodec = avcodec_find_encoder(AV_CODEC_ID_H264))) {
        LOGE("init h264 encoder failed");
        ret = -1;
        return end(NULL, outFormatCxt);
    }
//获取音频编码器aac
    if (!(avAudioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC))) {
//    if (!(avAudioCodec = avcodec_find_encoder_by_name("libfdk_aac"))) {
        LOGE("init aac encoder failed");
        ret = -1;
        return end(NULL, outFormatCxt);
    }

    outFormatCxt->oformat->video_codec = avVideoCodec->id;
    outFormatCxt->oformat->audio_codec = avAudioCodec->id;

    if (initVideoCodecContext() < 0)
        return end(NULL, outFormatCxt);

    if (initAudioCodecContext() < 0) {
        return end(NULL, outFormatCxt);
    }

    if (outFormatCxt->oformat->flags & AVFMT_GLOBALHEADER) {
        pVideoCodecCxt->flags |= CODEC_FLAG_GLOBAL_HEADER;
        pAudioCodecCxt->flags |= CODEC_FLAG_GLOBAL_HEADER;
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

    avformat_write_header(outFormatCxt, NULL);

    initSwrContext();
    pthread_mutex_init(&lock, NULL);
    pthread_mutex_init(&datalock, NULL);

    initAvVideoFrame(&avVideoFrame);

    init_samples_buffer(&inputSample, inputChannels, inputSampleFmt, pAudioCodecCxt->frame_size);
    initAvAudioFrame(&avAudioFrame);

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
    pAudioCodecCxt->sample_rate = avAudioCodec->supported_samplerates[0];
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
    avStream->codec = pVideoCodecCxt;
    avStream->time_base = pVideoCodecCxt->time_base;
    avcodec_parameters_from_context(avStream->codecpar, pVideoCodecCxt);
    videoStreamId = avStream->index;
    return avStream;
}

AVStream *initAudioStream() {
    AVStream *avStream = NULL;
    if (!(avStream = avformat_new_stream(outFormatCxt, avAudioCodec))) {
//    if (!(avStream = avformat_new_stream(outFormatCxt, NULL))) {
        LOGE("create audioStream failed");
        ret = -1;
        return NULL;
    }
    avStream->codec = pAudioCodecCxt;
    avStream->time_base = pAudioCodecCxt->time_base;
    avcodec_parameters_from_context(avStream->codecpar, pAudioCodecCxt);
    audioStreamId = avStream->index;
    return avStream;
}

int initAvVideoFrame(AVFrame **videoFrame){
    *videoFrame = av_frame_alloc();
    (*videoFrame)->format = pVideoCodecCxt->pix_fmt;
    (*videoFrame)->width = yuvWidth;
    (*videoFrame)->height = yuvHeight;
    return av_frame_get_buffer((*videoFrame), 32);
}

int initAvAudioFrame(AVFrame **avAudioFrame) {
    *avAudioFrame = av_frame_alloc();
    (*avAudioFrame)->format = pAudioCodecCxt->sample_fmt;
    (*avAudioFrame)->sample_rate = pAudioCodecCxt->sample_rate;
    (*avAudioFrame)->nb_samples = pAudioCodecCxt->frame_size;
    (*avAudioFrame)->pts = 0;
    (*avAudioFrame)->channels = pAudioCodecCxt->channels;
    (*avAudioFrame)->channel_layout = pAudioCodecCxt->channel_layout;
    return av_frame_get_buffer((*avAudioFrame), 0);
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
                                pAudioCodecCxt->channel_layout,
                                pAudioCodecCxt->sample_fmt,
                                pAudioCodecCxt->sample_rate,
                                av_get_default_channel_layout(inputChannels),
                                inputSampleFmt,
                                inputSampleRate,
                                0, NULL);
#endif
    swr_init(swrCxt);
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_encodeData(JNIEnv *env, jclass type,
                                                            jbyteArray bytes_, jbyteArray bytesU_,
                                                            jbyteArray bytesV_, jint rowStride,
                                                            jint pixelStride) {
    jbyte *yData = env->GetByteArrayElements(bytes_, NULL);
    jbyte *uData = env->GetByteArrayElements(bytesU_, NULL);
    jbyte *vData = env->GetByteArrayElements(bytesV_, NULL);
    // TODO
    int gotFrame = 0, gotSample = 0;
    int size = av_audio_fifo_size(fifo);
    LOGE("call back");
    int cmpRet = 0;
    bool hasSend = false;

    cmpRet = av_compare_ts(videoPts, pVideoCodecCxt->time_base, audioPts, pAudioCodecCxt->time_base);
    if((cmpRet) <= 0 ){
        analyzeYUVData((uint8_t *) yData, (uint8_t *) uData, (uint8_t *) vData, rowStride,
                       pixelStride);
        gotFrame = encodeYUV(&avVideoPacket);
        videoPts = avVideoFrame->pts;
        LOGE("videoPts:%lld", videoPts);
        if(gotFrame == 0){
            writeVideoFrame(&avVideoPacket);
        }
    }else{
        pthread_mutex_lock(&lock);
    while ((size = av_audio_fifo_size(fifo)) >= pAudioCodecCxt->frame_size) {
        LOGE("fifo size: %d", size);


            if (readAndConvert(fifo, inputSample, avAudioFrame) < 0)
                return AVERROR_EXIT;
            gotSample = encodeAudio(avAudioFrame, pAudioCodecCxt);
            audioPts = avAudioFrame->pts;
            LOGE("audioPts:%lld", audioPts);
            if (gotSample == 0) {
                writeAudioFrame(outFormatCxt);
                hasSend = true;
            }
        }
        pthread_mutex_unlock(&lock);
        analyzeYUVData((uint8_t *) yData, (uint8_t *) uData, (uint8_t *) vData, rowStride,
                       pixelStride);
        gotFrame = encodeYUV(&avVideoPacket);
        videoPts = avVideoFrame->pts;
        LOGE("videoPts:%lld", videoPts);
        if(gotFrame == 0){
            writeVideoFrame(&avVideoPacket);
        }
    }

    LOGE("call back end");
    env->ReleaseByteArrayElements(bytes_, yData, 0);
    env->ReleaseByteArrayElements(bytesU_, uData, 0);
    env->ReleaseByteArrayElements(bytesV_, vData, 0);
    return ret;
}

int encodeYUV(AVPacket *avVideoPacket) {
    avVideoFrame->pts = ++framecnt; /* * 1000 / 25*/;
    LOGE("frame pts:%lld", avVideoFrame->pts);
    avVideoPacket->data = NULL;
    avVideoPacket->size = 0;
    av_init_packet(avVideoPacket);
//    int g = 0;
//    int encGotFrame = avcodec_encode_video2(pVideoCodecCxt, &avVideoPacket, avVideoFrame, &g);
    ret = avcodec_send_frame(pVideoCodecCxt, avVideoFrame);
    if(ret < 0) {
        LOGError("avcodec_send_frame error:%d, %s", ret);
        return ret;
    }
    int encGotFrame = avcodec_receive_packet(pVideoCodecCxt, avVideoPacket);
    LOGE("avVideoPacket pts:%lld", avVideoPacket->pts);
    return encGotFrame;
}

void analyzeYUVData(uint8_t *y, uint8_t *u, uint8_t *v, int rowStride, int pixelStride) {
    int offset = 0;
    int length = yuvWidth;
    memset(avVideoFrame->data[0], 0, yuvHeight * yuvWidth);
    memset(avVideoFrame->data[1], 0, yuvWidth >>1);
    memset(avVideoFrame->data[2], 0, yuvWidth >>1);

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
}

void writeVideoFrame(AVPacket *avVideoPacket) {
    avVideoPacket->stream_index = videoStreamId;
    AVRational frameRate = {25, 1};
    avVideoPacket->pts = av_rescale(avVideoPacket->pts, outFormatCxt->streams[0]->time_base.den,
                                    20);
    avVideoPacket->dts = av_rescale(avVideoPacket->dts, outFormatCxt->streams[0]->time_base.den,
                                    20);
    avVideoPacket->duration =
            (AV_TIME_BASE) / av_q2d(frameRate) / outFormatCxt->streams[0]->time_base.den;
    avVideoPacket->pos = -1;
//    av_packet_rescale_ts(avVideoPacket, pVideoCodecCxt->time_base, outFormatCxt->streams[0]->time_base);
    int64_t ptsTime = av_rescale_q(avVideoPacket->dts, outFormatCxt->streams[0]->time_base,
                                   (AVRational) {1, 25});
    int64_t nowTime = av_gettime() - startTime;
    if (ptsTime > nowTime) {
        LOGE("sleeptime: %lld", ptsTime - nowTime);
        av_usleep(ptsTime - nowTime);
    }

    LOGE("video packet dts:%lld, pts:%lld", avVideoPacket->dts, avVideoPacket->pts);
        ret = av_interleaved_write_frame(outFormatCxt, avVideoPacket);
//    ret = av_write_frame(outFormatCxt, avVideoPacket);
    if (ret < 0) {
        static char error_buffer[255];
        av_strerror(ret, error_buffer, sizeof(error_buffer));
        LOGE("write video frame ret: %d, %s", ret, error_buffer);
    }

    av_packet_unref(avVideoPacket);
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_saveAudioBuffer(JNIEnv *env, jclass type,
                                                                 jbyteArray bytes_, jint length) {
    jbyte *bytes = env->GetByteArrayElements(bytes_, NULL);

    // TODO
    int byteSize = inputSampleFmt == AV_SAMPLE_FMT_FLT ? sizeof(float) : sizeof(uint16_t);
    int size = length / inputChannels / byteSize;
    pthread_mutex_lock(&lock);
    int ret = add_samples_to_fifo(fifo, (uint8_t **) &bytes, size);
    pthread_mutex_unlock(&lock);
    env->ReleaseByteArrayElements(bytes_, bytes, 0);
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
    av_free(&inputSample[0]);
    free(inputSample);
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



//JNIEXPORT jint JNICALL
//Java_com_mikiller_ndktest_ndkapplication_NDKImpl_encodePCM(JNIEnv *env, jclass type) {
//    pthread_mutex_lock(&lock);
////    jbyte *bytes = env->GetByteArrayElements(bytes_, NULL);
//
//    // TODO
////    int byteSize = inputSampleFmt == AV_SAMPLE_FMT_FLT ? sizeof(float) : sizeof(uint16_t);
////    int size = length / inputChannels / byteSize;
////    LOGE("input nb_samples:%d", size);
////    LOGE("datalock");
////    pthread_mutex_lock(&datalock);
////    add_samples_to_fifo(fifo, (uint8_t **) &bytes, size);
//    int size = 0;
//    size = av_audio_fifo_size(fifo);
//    LOGE("fifo size before: %d", size);
//    while ((size = av_audio_fifo_size(fifo)) >= pAudioCodecCxt->frame_size) {
//        LOGE("fifo size: %d", size);
//        pthread_mutex_lock(&datalock);
//        const int frame_size = FFMIN(size,
//                                     pAudioCodecCxt->frame_size);
//        for (int i = 0; i < inputChannels; i++) {
//            memset(inputSample[i], 0, avAudioFrame->linesize[0]);
//        }
//        LOGE("read and write ");
//        if (convert_and_read_and_write(fifo, inputSample, avAudioFrame, pAudioCodecCxt,
//                                       outFormatCxt, frame_size)) {
//            LOGE("read and write failed");
//            pthread_mutex_unlock(&datalock);
//            //pthread_mutex_unlock(&lock);
//            //break;
//        }
//        LOGE("read and write end");
//        pthread_mutex_unlock(&datalock);
//    }
////    pthread_mutex_unlock(&datalock);
//    LOGE("dataunlock");
////        if(sendFrame) {
////            int gotFrame = 0;
////            do {
////                if (encode_audio_frame(NULL, outFormatCxt, pAudioCodecCxt, &gotFrame))
////                    break;
////            } while (gotFrame);
////            //break;
////        }
//
////    env->ReleaseByteArrayElements(bytes_, bytes, 0);
//    pthread_mutex_unlock(&lock);
//    return 0;
//}


//JNIEXPORT jint JNICALL
//Java_com_mikiller_ndktest_ndkapplication_NDKImpl_encodeYUV(JNIEnv *env, jclass type,
//                                                           jbyteArray yuvData_) {
//    jbyte *yuvData = env->GetByteArrayElements(yuvData_, NULL);
//
//    // TODO
//
//    if (pVideoCodecCxt == NULL) {
//        ret = -1;
//        return end(NULL, outFormatCxt);
//    }
//
//    av_image_fill_arrays(avVideoFrame->data, avVideoFrame->linesize, frameBuffer,
//                         pVideoCodecCxt->pix_fmt,
//                         yuvWidth, yuvHeight, 1);
//
//    //convert android camera data from NV21 to yuv420p
//    memcpy(avVideoFrame->data[0], yuvData, yLength);
//    for (int i = 0; i < uvLenght; i++) {
//        *(avVideoFrame->data[2] + i) = *(yuvData + yLength + i * 2);
//        *(avVideoFrame->data[1] + i) = *(yuvData + yLength + i * 2 + 1);
//    }
//
//    writeFrame();
//    env->ReleaseByteArrayElements(yuvData_, yuvData, 0);
//    return ret;
//}

//
//JNIEXPORT jint JNICALL
//Java_com_mikiller_ndktest_ndkapplication_NDKImpl_encodeYUV1(JNIEnv *env, jclass type,
//                                                            jbyteArray yData_, jbyteArray uData_,
//                                                            jbyteArray vData_,
//                                                            jint rowStride, jint pixelStride) {
//    pthread_mutex_lock(&datalock);
//    jbyte *yData = env->GetByteArrayElements(yData_, NULL);
//    jbyte *uData = env->GetByteArrayElements(uData_, NULL);
//    jbyte *vData = env->GetByteArrayElements(vData_, NULL);
//
//    // TODO
//    if (pVideoCodecCxt == NULL) {
//        ret = -1;
//        return end(NULL, outFormatCxt);
//    }
//
//    analyzeYUVData((uint8_t *) yData, (uint8_t *) uData, (uint8_t *) vData, rowStride, pixelStride);
//    writeFrame();
//    env->ReleaseByteArrayElements(yData_, yData, 0);
//    env->ReleaseByteArrayElements(uData_, uData, 0);
//    env->ReleaseByteArrayElements(vData_, vData, 0);
//    pthread_mutex_unlock(&datalock);
//    return ret;
//}

//void writeFrame() {
//    avVideoFrame->format = AV_PIX_FMT_YUV420P;
//    avVideoFrame->width = yuvWidth;
//    avVideoFrame->height = yuvHeight;
//    avVideoFrame->pts = ++framecnt; /* * 1000 / 25*/;
//
//    avVideoPacket.data = NULL;
//    avVideoPacket.size = 0;
//    av_init_packet(&avVideoPacket);
////    int g = 0;
////    int encGotFrame = avcodec_encode_video2(pVideoCodecCxt, &avVideoPacket, avVideoFrame, &g);
//    ret = avcodec_send_frame(pVideoCodecCxt, avVideoFrame);
//    int encGotFrame = avcodec_receive_packet(pVideoCodecCxt, &avVideoPacket);
//
//    if (encGotFrame == 0) {
////        avVideoPacket.stream_index = outFormatCxt->streams[0]->index;
//        avVideoPacket.stream_index = videoStreamId;
//        AVRational frameRate = {25, 1};
//        avVideoPacket.pts = av_rescale(avVideoPacket.pts, outFormatCxt->streams[0]->time_base.den,
//                                       20);
//        avVideoPacket.dts = av_rescale(avVideoPacket.dts, outFormatCxt->streams[0]->time_base.den,
//                                       20);
//        avVideoPacket.duration =
//                (AV_TIME_BASE) / av_q2d(frameRate) / outFormatCxt->streams[0]->time_base.den;
//        avVideoPacket.pos = -1;
//        //av_packet_rescale_ts(&avVideoPacket, pVideoCodecCxt->time_base, outFormatCxt->streams[0]->time_base);
////        LOGE("NDK pkt pts: %lld, dts: %lld", avVideoPacket.pts, avVideoPacket.dts);
//        int64_t ptsTime = av_rescale_q(avVideoPacket.dts, outFormatCxt->streams[0]->time_base,
//                                       (AVRational) {1, 25});
//        int64_t nowTime = av_gettime() - startTime;
////        LOGE("starttime1: %lld", startTime);
////        LOGE("ptstime: %lld, nowtime: %lld", ptsTime, nowTime);
//        if (ptsTime > nowTime) {
//            LOGE("sleeptime: %lld", ptsTime - nowTime);
//            av_usleep(ptsTime - nowTime);
//        }
//
//        LOGE("video packet dts:%lld", avVideoPacket.dts);
////        ret = av_interleaved_write_frame(outFormatCxt, &avVideoPacket);
//        ret = av_write_frame(outFormatCxt, &avVideoPacket);
//        if (ret < 0) {
//            static char error_buffer[255];
//            av_strerror(ret, error_buffer, sizeof(error_buffer));
//            LOGE("write video frame ret: %d, %s", ret, error_buffer);
//        }
//
//        av_packet_unref(&avVideoPacket);
//    }
//}
}
