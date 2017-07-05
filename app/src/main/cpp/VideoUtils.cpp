//
// Created by Mikiller on 2017/1/16.
//
#ifndef NDKTEST_VIDEOUTILS_CPP
#define NDKTEST_VIDEOUTILS_CPP

#include <pthread.h>
#include "VideoUtils.h"
#include "libyuv.h"

AVCodecContext *videoCodecCxt = NULL;
AVCodec *videoCodec = NULL;
AVFrame *videoFrame = NULL;
AVPacket avVideoPacket;
int videoStreamId = 0;
int64_t videoPts = 0;
int rotate, in_width, in_height;
unsigned char *I420;
unsigned char *pDstY;
unsigned char *pDstU;
unsigned char *pDstV;
int Ysize;
size_t src_size;

AVCodecID getVideoCodecId() {
    if (!(videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264))) {
        LOGE("init h264 encoder failed");
        return AV_CODEC_ID_NONE;
    }
    return videoCodec->id;
}

AVCodecContext *initVideoCodecContext(int orientation, int width, int height, int bitRate,
                                      int fps) {
    if (!(videoCodecCxt = avcodec_alloc_context3(videoCodec))) {
        LOGE("init avVideoContext failed");
        return NULL;
    }
    rotate = orientation;
    Ysize = width * height;
    src_size = Ysize * 1.5;

    videoCodecCxt->pix_fmt = AV_PIX_FMT_YUV420P;
    videoCodecCxt->width = rotate == 0 ? width : height;
    videoCodecCxt->height = rotate == 0 ? height : width;
    videoCodecCxt->time_base = av_inv_q(av_d2q(fps, 100000));
    videoCodecCxt->bit_rate = bitRate; //传输速率 rate/kbps

    videoCodecCxt->rc_max_rate = bitRate;
    videoCodecCxt->rc_min_rate = bitRate;
    videoCodecCxt->rc_buffer_size = bitRate / 2;
    videoCodecCxt->bit_rate_tolerance = bitRate;
    videoCodecCxt->rc_initial_buffer_occupancy = videoCodecCxt->rc_buffer_size * 3 / 4;

    videoCodecCxt->gop_size = 12; //gop = fps/N ?
    videoCodecCxt->max_b_frames = 8; // fps?
    videoCodecCxt->delay = 0;
    videoCodecCxt->thread_type = FF_THREAD_FRAME;
    videoCodecCxt->thread_count = 2;
    //videoCodecCxt->qmin = 25;
//    videoCodecCxt->qmax = 33; // 1-51, 10-30 is better
    videoCodecCxt->profile = FF_PROFILE_H264_BASELINE;
    videoCodecCxt->flags |= CODEC_FLAG_GLOBAL_HEADER;

    in_width = rotate == 0 ? videoCodecCxt->width : videoCodecCxt->height;
    in_height = rotate == 0 ? videoCodecCxt->height : videoCodecCxt->width;
    return videoCodecCxt;
}

int openVideoEncoder() {
    AVDictionary *options = NULL;
    av_dict_set(&options, "preset", "ultrafast", 0);
    av_dict_set(&options, "tune", "zerolatency", 0);
//    av_dict_set(&param, "profile", "baseline", 0);
    if (videoCodec->capabilities & CODEC_CAP_EXPERIMENTAL != 0) {
        videoCodecCxt->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    }
    ret = avcodec_open2(videoCodecCxt, videoCodec, &options);
    av_dict_free(&options);
    return ret;
}

int getVideoStreamId(AVFormatContext *outFormatCxt) {
    AVStream *avStream = NULL;
    if (!(avStream = avformat_new_stream(outFormatCxt, videoCodec))) {
        LOGE("create avStream failed");
        return -1;
    }
    avStream->time_base = videoCodecCxt->time_base;
    avcodec_parameters_from_context(avStream->codecpar, videoCodecCxt);
    return videoStreamId = avStream->index;
}

int initAvVideoFrame() {
    videoFrame = av_frame_alloc();
    videoFrame->format = videoCodecCxt->pix_fmt;
    videoFrame->width = videoCodecCxt->width;
    videoFrame->height = videoCodecCxt->height;
    return av_frame_get_buffer(videoFrame, 32);
}

int analyzeNV21Data(uint8_t *data) {
    if (I420 == NULL) {
        I420 = new unsigned char[src_size];
        pDstY = I420;
        pDstU = I420 + Ysize;
        pDstV = pDstU + (Ysize / 4);
    } else {
        memset(I420, 0, src_size);
    }
    return libyuv::ConvertToI420(data, src_size,
                                 pDstY, videoCodecCxt->width,
                                 pDstU, videoCodecCxt->width / 2,
                                 pDstV, videoCodecCxt->width / 2,
                                 0, 0,
                                 in_width, in_height,
                                 in_width, in_height,
                                 (libyuv::RotationMode) rotate, libyuv::FOURCC_NV21);
}

int encodeYUV(jboolean isPause, int needFrame) {
    if(needFrame) {
        if(isPause != JNI_TRUE) {
            av_image_fill_arrays(videoFrame->data, videoFrame->linesize, I420, AV_PIX_FMT_YUV420P,
                                 videoCodecCxt->width, videoCodecCxt->height, 1);
            videoFrame->pts = videoPts++;
        }
        ret = avcodec_send_frame(videoCodecCxt, videoFrame);

    }else{
        ret = avcodec_send_frame(videoCodecCxt, NULL);
        videoPts = 0;
        free(I420);
        I420 = NULL;
    }
    if (ret < 0) {
        LOGError("avcodec_send_video_frame error:%d, %s", ret);
        av_packet_unref(&avVideoPacket);
        return ret;
    }
    av_init_packet(&avVideoPacket);
    avVideoPacket.data = NULL;
    avVideoPacket.size = 0;
    if ((ret = avcodec_receive_packet(videoCodecCxt, &avVideoPacket)) < 0) {
        av_packet_unref(&avVideoPacket);
    }
    avVideoPacket.stream_index = videoStreamId;
    avVideoPacket.flags |= AV_PKT_FLAG_KEY;
    avVideoPacket.duration = 1000 / 10;
    return ret;
}

int writeVideoFrame(AVFormatContext *outFormatCxt, pthread_mutex_t *datalock) {
    if (avVideoPacket.pts != AV_NOPTS_VALUE) {
        avVideoPacket.pts = TSCalculate() - ts;
        avVideoPacket.dts = avVideoPacket.pts;
    }
    pthread_mutex_lock(datalock);
    ret = av_interleaved_write_frame(outFormatCxt, &avVideoPacket);
    if (ret < 0) {
        LOGError("write video frame ret: %d, %s", ret);
    }
    pthread_mutex_unlock(datalock);
    av_packet_unref(&avVideoPacket);
    return ret;
}

void flushVideo(AVFormatContext *outFormatCxt, pthread_mutex_t *datalock) {
    while (encodeYUV(false, false) == 0) {
        writeVideoFrame(outFormatCxt, datalock);
        LOGError("flush video : %d, %s", ret);
    }
}

void freeVideoReference() {
    if (videoCodecCxt != NULL && avcodec_is_open(videoCodecCxt)) {
        LOGE("free vidio codeccontext");
        avcodec_free_context(&videoCodecCxt);
        LOGE("end");
    }

}


#endif