//
// Created by Mikiller on 2016/11/25.
//

#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include "NDKImpl.h"

extern "C" {
#include "AudioUtils.h"
#include "VideoUtils.h"
#include "libavfilter/avfilter.h"
#include "libavdevice/avdevice.h"

int videoStreamId = 0, audioStreamId = 0;
int64_t startTime = 0;
int64_t videoPts = 0, audioPts = 0;

pthread_mutex_t lock;
pthread_mutex_t datalock;

JNIEXPORT jstring JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_helloWorld(JNIEnv *env, jobject instance,
                                                            jstring input_) {
    const char *input = env->GetStringUTFChars(input_, 0);

    // TODO

    env->ReleaseStringUTFChars(input_, input);

    return env->NewStringUTF(input);
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
        return end(&inputFormatContext, &outputFormatContext);
    }
    //input from file
    if ((ret = avformat_open_input(&inputFormatContext, input, 0, 0)) < 0) {
        LOGE("input file isnot exist");
        return end(&inputFormatContext, &outputFormatContext);
    }

    if ((ret = avformat_find_stream_info(inputFormatContext, 0)) < 0) {
        LOGE("failed to get input information");
        return end(&inputFormatContext, &outputFormatContext);
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
            return end(&inputFormatContext, &outputFormatContext);
        }
//        ret = avcodec_copy_context(outputStream->codec, inputStream->codec);
        ret = avcodec_parameters_copy(outputStream->codecpar, inputStream->codecpar);
        //        ret = avcodec_parameters_from_context(outputStream->codecpar, inputStream->codec);
        if (ret < 0) {
            LOGE("copy faield");
            ret = -1;
            return end(&inputFormatContext, &outputFormatContext);
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
            return end(&inputFormatContext, &outputFormatContext);
        }
    }

    ret = avformat_write_header(outputFormatContext, NULL);
    if (ret < 0) {
        LOGE("write open url head failed");
        ret = -1;
        return end(&inputFormatContext, &outputFormatContext);
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
    int rst = end(&inputFormatContext, &outputFormatContext);
    return rst;
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_initFFMpeg(JNIEnv *env, jclass type,
                                                            jstring outputUrl_, jint width,
                                                            jint height,
                                                            jint channels, jint sampleFmt,
                                                            jint sampleRate) {
    const char *outputUrl = env->GetStringUTFChars(outputUrl_, 0);
    AVCodecContext *pVideoCodecCxt = NULL;
    AVCodecContext *pAudioCodecCxt = NULL;
    // TODO
    registFFMpeg();

    if ((ret = init_and_open_outFormatCxt(&outFormatCxt, outputUrl, getVideoCodecId(),
                                          getAudioCodecId())) < 0) {
        LOGError("open outFormatCxt failed ret:%d, %s", ret);
        return end(NULL, &outFormatCxt);
    }

    initYUVSize(width, height);
    initSampleParams(channels, sampleFmt, sampleRate);

    if (!(pVideoCodecCxt = initVideoCodecContext()))
        return end(NULL, &outFormatCxt);

    if (!(pAudioCodecCxt = initAudioCodecContext())) {
        return end(NULL, &outFormatCxt);
    }

    if (outFormatCxt->oformat->flags & AVFMT_GLOBALHEADER) {
        pVideoCodecCxt->flags |= CODEC_FLAG_GLOBAL_HEADER;
        pAudioCodecCxt->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    if ((ret = openVideoEncoder()) < 0) {
        LOGError("open video encoder failed ret:%d, %s", ret);
        return end(NULL, &outFormatCxt);
    }

    if ((ret = openAudioEncoder()) < 0) {
        LOGError("open audio encoder failed ret:%d, %s", ret);
        return end(NULL, &outFormatCxt);
    }

    AVStream *avStream = initAvVideoStream(outFormatCxt, &videoStreamId);
    if (!avStream) {
        return end(NULL, &outFormatCxt);
    }
    AVStream *audioStream = initAudioStream(outFormatCxt, &audioStreamId);
    if (!audioStream) {
        return end(NULL, &outFormatCxt);
    }

    avformat_write_header(outFormatCxt, NULL);


    initAvVideoFrame();
    initAvAudioFrame();

    initSwrContext();
    init_samples_buffer();

    pthread_mutex_init(&lock, NULL);
    pthread_mutex_init(&datalock, NULL);

    env->ReleaseStringUTFChars(outputUrl_, outputUrl);
    return 0;
}


void registFFMpeg() {
    av_log_set_callback(custom_log);

    av_register_all();
    avcodec_register_all();

    avformat_network_init();
}

int init_and_open_outFormatCxt(AVFormatContext **outFmtCxt, const char *outPath,
                               AVCodecID videoCodecId, AVCodecID audioCodecId) {
    if ((videoCodecId | audioCodecId) == AV_CODEC_ID_NONE)
        return AVERROR(AVERROR_ENCODER_NOT_FOUND);
    avformat_alloc_output_context2(outFmtCxt, NULL, "flv", outPath);
    (*outFmtCxt)->oformat->video_codec = videoCodecId;
    (*outFmtCxt)->oformat->audio_codec = audioCodecId;
    return avio_open(&outFormatCxt->pb, outPath, AVIO_FLAG_WRITE);
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
    pthread_mutex_lock(&lock);
    int gotFrame = 0, gotSample = 0;
    int cmpRet = 0;

    cmpRet = av_compare_ts(videoPts, getVideoTimebase(), audioPts, getAudioTimebase());
    if (cmpRet > 0) {
        pthread_mutex_lock(&datalock);
        while (needWriteAudioFrame()) {

            if (readAndConvert() < 0)
                return AVERROR_EXIT;
            gotSample = encodeAudio(&audioPts);
            if (gotSample == 0) {
                writeAudioFrame(outFormatCxt, audioStreamId, startTime);
            } else {
                LOGError("encode audio ret:%d, %s", gotSample);
            }
        }
        pthread_mutex_unlock(&datalock);
    }

    int needFrame = adjustFrame();
    if(needFrame >= 0){
        analyzeYUVData((uint8_t *) yData, (uint8_t *) uData, (uint8_t *) vData, rowStride,
                       pixelStride);

        for(int i = 0; i<=needFrame; i++){
            gotFrame = encodeYUV(&videoPts);
            if (gotFrame == 0) {
                writeVideoFrame(outFormatCxt, videoStreamId, startTime);
                LOGE("needframe: %d, time: %lld",needFrame,  av_gettime( ) / 1000);
            } else {
                LOGError("encode video ret:%d, %s", gotFrame);
            }
        }
    }
//    analyzeYUVData((uint8_t *) yData, (uint8_t *) uData, (uint8_t *) vData, rowStride,
//                   pixelStride);
//    gotFrame = encodeYUV(&videoPts);
//    if (gotFrame == 0) {
//        writeVideoFrame(outFormatCxt, videoStreamId, startTime);
//    } else {
//        LOGError("encode video ret:%d, %s", gotFrame);
//    }

    pthread_mutex_unlock(&lock);
    env->ReleaseByteArrayElements(bytes_, yData, 0);
    env->ReleaseByteArrayElements(bytesU_, uData, 0);
    env->ReleaseByteArrayElements(bytesV_, vData, 0);

    return ret;
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_saveAudioBuffer(JNIEnv *env, jclass type,
                                                                 jbyteArray bytes_, jint length) {
    jbyte *bytes = env->GetByteArrayElements(bytes_, NULL);

    // TODO
    pthread_mutex_lock(&datalock);
    int ret = add_samples_to_fifo((uint8_t **) &bytes, length);
    pthread_mutex_unlock(&datalock);
    env->ReleaseByteArrayElements(bytes_, bytes, 0);
    return ret;
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_flush(JNIEnv *env, jclass type) {
    flushVideo(outFormatCxt, videoStreamId);
    return 0;
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_close(JNIEnv *env, jclass type) {

    // TODO
    if (outFormatCxt)
        av_write_trailer(outFormatCxt);
    end(NULL, &outFormatCxt);
    return 0;
}

jint end(AVFormatContext **inputFormatContext, AVFormatContext **outputFormatContext) {
    if (inputFormatContext != NULL) {
        avformat_close_input(inputFormatContext);
        *inputFormatContext = NULL;
    }
    if (*outputFormatContext) {
        if ((*outputFormatContext)->oformat &&
            !((*outputFormatContext)->oformat->flags & AVFMT_NOFILE)) {
            avio_close((*outputFormatContext)->pb);
        }
        avformat_free_context(*outputFormatContext);
        *outputFormatContext = NULL;
        LOGE("free outputFormat");
    }

    freeVideoReference();
    freeAudioReference();

    if (ret < 0 && ret == AVERROR_EOF) {
        LOGE("unknow error");
        return -1;
    }
    pthread_mutex_destroy(&lock);
    pthread_mutex_destroy(&datalock);
    startTime = 0;
    return ret;
}

JNIEXPORT void JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_initStartTime(JNIEnv *, jclass) {
    startTime = av_gettime();
}

};