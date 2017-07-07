#include <jni.h>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavformat/avformat.h>
//#include "native-lib.h"


jstring
Java_com_mikiller_ndktest_ndkapplication_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
//    std::string hello = "Hello from C++";
//    return env->NewStringUTF(hello.c_str());

//    char info[10000] = {0};
//    sprintf(info, "%s\n", avcodec_configuration());

    char info[40000] = {0};
    av_register_all();
    sprintf(info, "%s\n", avcodec_configuration());
    AVCodec *c_temp = av_codec_next(NULL);
    while (c_temp != NULL) {
        if (c_temp->decode != NULL) {
            sprintf(info, "%sdecode:", info);
        } else {
            sprintf(info, "%sencode:", info);
        }
        switch (c_temp->type) {
            case AVMEDIA_TYPE_VIDEO:
                sprintf(info, "%s(video):", info);
                break;
            case AVMEDIA_TYPE_AUDIO:
                sprintf(info, "%s(audio):", info);
                break;
            default:
                sprintf(info, "%s(other):", info);
                break;
        }
        sprintf(info, "%s[%10s]\n", info, c_temp->name);
        c_temp = c_temp->next;
    }
    return env->NewStringUTF(info);
}

}
/**
 * use extern "C"
 * or use #include "*.h"
 * */
//JNIEXPORT jstring JNICALL
//Java_com_mikiller_ndktest_ndkapplication_NDKImpl_helloWorld(JNIEnv *env, jobject instance,
//                                                            jstring input_) {
//    const char *input = env->GetStringUTFChars(input_, 0);
//
//    // TODO
//
//    env->ReleaseStringUTFChars(input_, input);
//
//    return env->NewStringUTF(input);
//}