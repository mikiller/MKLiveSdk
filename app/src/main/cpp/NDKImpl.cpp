//
// Created by Mikiller on 2016/11/25.
//

#include <stdio.h>
#include <pthread.h>
#include "NDKImpl.h"

extern "C" {
#include "AudioUtils.h"
#include "VideoUtils.h"

char *outputUrl;
pthread_mutex_t lock;
pthread_mutex_t audiolock;
pthread_mutex_t datalock;

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_initFFMpeg(JNIEnv *env, jclass type,
                                                            jstring outputUrl_, jint orientation,
                                                            jint width, jint height,
                                                            jint channels,
                                                            jint videoBitRate, jint audioBitRate) {
    outputUrl = (char *) env->GetStringUTFChars(outputUrl_, 0);
    // TODO
    registFFMpeg();
    if ((ret = init_and_open_outFormatCxt(&outFormatCxt, getVideoCodecId(),
                                          getAudioCodecId())) < 0) {
        LOGError("open outFormatCxt failed ret:%d, %s", ret);
        return end(NULL, &outFormatCxt);
    }
    if (!(initVideoCodecContext(orientation, width, height, videoBitRate, 24))) {
        return end(NULL, &outFormatCxt);
    }
    if (!(initAudioCodecContext(audioBitRate, channels))) {
        return end(NULL, &outFormatCxt);
    }
    if ((ret = openVideoEncoder()) < 0) {
        LOGError("open video encoder failed ret:%d, %s", ret);
        return end(NULL, &outFormatCxt);
    }
    if ((ret = openAudioEncoder("aac_lc")) < 0) {
        LOGError("open audio encoder failed ret:%d, %s", ret);
        return end(NULL, &outFormatCxt);
    }
    if (getVideoStreamId(outFormatCxt) < 0) {
        return end(NULL, &outFormatCxt);
    }
    if (getAudioStreamId(outFormatCxt) < 0) {
        return end(NULL, &outFormatCxt);
    }

    avformat_write_header(outFormatCxt, NULL);

    initAvVideoFrame();
    initAvAudioFrame();

    pthread_mutex_init(&lock, NULL);
    pthread_mutex_init(&audiolock, NULL);
    pthread_mutex_init(&datalock, NULL);

    audioPts = 0;
    ts = TSCalculate();

    env->ReleaseStringUTFChars(outputUrl_, outputUrl);
    LOGE("init ffmpeg end");
    return 0;
}


void registFFMpeg() {
    av_log_set_callback(custom_log);

    av_register_all();
    avcodec_register_all();

    avformat_network_init();
}

int init_and_open_outFormatCxt(AVFormatContext **outFmtCxt,
                               AVCodecID videoCodecId, AVCodecID audioCodecId) {
    if ((videoCodecId | audioCodecId) == AV_CODEC_ID_NONE)
        return AVERROR(AVERROR_ENCODER_NOT_FOUND);
    ret = avformat_alloc_output_context2(outFmtCxt, NULL, "flv", outputUrl);
    (*outFmtCxt)->oformat->video_codec = videoCodecId;
    (*outFmtCxt)->oformat->audio_codec = audioCodecId;
    ret = avio_open(&outFormatCxt->pb, outputUrl, AVIO_FLAG_READ_WRITE | AVIO_FLAG_NONBLOCK);
    return ret;
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_pushVideo(JNIEnv *env, jclass type,
                                                            jbyteArray bytes_, jboolean isPause) {
    jint dataLength = env->GetArrayLength(bytes_);
    jbyte *jBuffer = (jbyte *) malloc(dataLength * sizeof(jbyte));
    env->GetByteArrayRegion(bytes_, 0, dataLength, jBuffer);

    pthread_mutex_lock(&lock);
    if(!isPause && (ret = analyzeNV21Data((uint8_t *) jBuffer) < 0)){
        LOGError("analyzeNV21Data failed: %d, %s", ret);
    }else if ((ret = encodeYUV(isPause)) < 0) {
        LOGError("encodeYUV failed: %d, %s", ret);
    }else{
        ret = writeVideoFrame(outFormatCxt, &datalock);
    }
    av_usleep(1);
    pthread_mutex_unlock(&lock);

    free(jBuffer);
    return ret;
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_pushAudio(JNIEnv *env, jclass type,
                                                                 jbyteArray bytes_) {
    jbyte *data = env->GetByteArrayElements(bytes_, NULL);

    // TODO
    pthread_mutex_lock(&audiolock);
    if((ret = encodeAudio((uint8_t *) data)) < 0)
        LOGError("encode adataudio failed: %d, %s", ret);
    else {
        ret = writeAudioFrame(outFormatCxt, &datalock);
    }
    av_usleep(1);
    pthread_mutex_unlock(&audiolock);
    env->ReleaseByteArrayElements(bytes_, data, 0);
    return ret;
}

JNIEXPORT jint JNICALL
Java_com_mikiller_ndktest_ndkapplication_NDKImpl_flush(JNIEnv *env, jclass type) {
    pthread_mutex_lock(&lock);
    flushVideo(outFormatCxt, &datalock);
    pthread_mutex_unlock(&lock);
    pthread_mutex_lock(&audiolock);
    flushAudio(outFormatCxt, &datalock);
    pthread_mutex_unlock(&audiolock);
    return 0;
}

JNIEXPORT void JNICALL Java_com_mikiller_ndktest_ndkapplication_NDKImpl_initTS
        (JNIEnv *, jclass) {
    ts = TSCalculate();
}

JNIEXPORT void JNICALL Java_com_mikiller_ndktest_ndkapplication_NDKImpl_setRotate
        (JNIEnv *, jclass, jint rotate){
    setRotate(rotate);
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

    LOGError("final error: %d, %s", ret);
    if (ret == AVERROR_EOF) {
        ret = 0;
    }
    pthread_mutex_destroy(&lock);
    pthread_mutex_destroy(&datalock);
    pthread_mutex_destroy(&audiolock);
    return ret;
}

};