//
// Created by Mikiller on 2016/12/8.
//

#ifndef NDKTEST_PRIVATEUTILS_H
#define NDKTEST_PRIVATEUTILS_H
extern "C"{
#ifdef ANDROID

#include <jni.h>
#include <android/log.h>

#define TAG "NDKIMPL"

#define LOGE(format, ...)  __android_log_print(ANDROID_LOG_ERROR, TAG, format, ##__VA_ARGS__)
#define LOGI(format, ...)  __android_log_print(ANDROID_LOG_INFO,  TAG, format, ##__VA_ARGS__)
#else
#define LOGE(format, ...)  printf(TAG format "\n", ##__VA_ARGS__)
#define LOGI(format, ...)  printf(TAG format "\n", ##__VA_ARGS__)
#endif


#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/time.h>

static AVFormatContext *outFormatCxt = NULL;
static int ret = 0;

static void custom_log(void *ptr, int level, const char *fmt, va_list vl) {
    FILE *file = fopen("/storage/emulated/0/av_log.txt", "a+");
    if (file) {
        vfprintf(file, fmt, vl);
        fflush(file);
        fclose(file);
    }
}

static void LOGError(char *format, int ret) {
    static char error_buffer[255];
    av_strerror(ret, error_buffer, sizeof(error_buffer));
    LOGE(format, ret, error_buffer);
}

static long long TSCalculate()
{
    struct timeval tsvalue;
    struct timezone zone;
    gettimeofday(&tsvalue, &zone);
    long long tsvalue_present = (tsvalue.tv_sec * 1000ll) + (tsvalue.tv_usec / 1000ll);
    return tsvalue_present;
}

void registFFMpeg();

int init_and_open_outFormatCxt(AVFormatContext **, char*, enum AVCodecID, enum AVCodecID);

jint end(AVFormatContext **inputFormatContext, AVFormatContext **outputFormatContext);

};
#endif //NDKTEST_PRIVATEUTILS_H
