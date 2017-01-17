//
// Created by Mikiller on 2017/1/16.
//
#ifndef NDKTEST_VIDEOUTILS_CPP
#define NDKTEST_VIDEOUTILS_CPP

#include "VideoUtils.h"

AVCodecContext *videoCodecCxt = NULL;
AVCodec *videoCodec = NULL;
AVFrame *videoFrame = NULL;
AVPacket avVideoPacket;
AVRational videoTimebase = {1, 15};
int yuvWidth = 0, yuvHeight = 0;
int64_t firstTime = 0;
void initYUVSize(int w, int h){
    yuvWidth = w;
    yuvHeight = h;
}

AVCodecID getVideoCodecId(){
    if(!(videoCodec = avcodec_find_encoder(AV_CODEC_ID_H264))){
        LOGE("init h264 encoder failed");
        return AV_CODEC_ID_NONE;
    }
    return videoCodec->id;
}

AVCodecContext* initVideoCodecContext() {
    if (!(videoCodecCxt = avcodec_alloc_context3(videoCodec))) {
        LOGE("init avVideoContext failed");
        return NULL;
    }
    videoCodecCxt->pix_fmt = AV_PIX_FMT_YUV420P;
    videoCodecCxt->width = yuvWidth;
    videoCodecCxt->height = yuvHeight;
    videoCodecCxt->time_base = videoTimebase;
    videoCodecCxt->bit_rate = 800 * 1000; //传输速率 rate/kbps
//    videoCodecCxt->gop_size = 12; //gop = fps/N ?
//    videoCodecCxt->max_b_frames = 3; // fps?
    videoCodecCxt->thread_type = FF_THREAD_FRAME;
    videoCodecCxt->thread_count = 4;
//    videoCodecCxt->qmin = 1; // 1-51, 10-30 is better
//    videoCodecCxt->qmax = 35; // 1-51, 10-30 is better
//    videoCodecCxt->profile = FF_PROFILE_H264_HIGH;
    return videoCodecCxt;
}

int openVideoEncoder(){
    AVDictionary *param = NULL;
    av_dict_set(&param, "preset", "ultrafast"/*"superfast"*/, 0);
    av_dict_set(&param, "tune", "zerolatency", 0);
    return avcodec_open2(videoCodecCxt, videoCodec, &param);
//    return avcodec_open2(videoCodecCxt, videoCodec, NULL);
}

AVStream *initAvVideoStream(AVFormatContext *outFormatCxt, int *videoStreamId) {
    AVStream *avStream = NULL;
    if (!(avStream = avformat_new_stream(outFormatCxt, videoCodec))) {
        LOGE("create avStream failed");
        return NULL;
    }
    avStream->codec = videoCodecCxt;
    avStream->time_base = videoCodecCxt->time_base;
    avcodec_parameters_from_context(avStream->codecpar, videoCodecCxt);
    *videoStreamId = avStream->index;
    return avStream;
}

int initAvVideoFrame() {
    videoFrame = av_frame_alloc();
    videoFrame->format = videoCodecCxt->pix_fmt;
    videoFrame->width = yuvWidth;
    videoFrame->height = yuvHeight;
    return av_frame_get_buffer(videoFrame, 32);
}

AVRational getVideoTimebase(){
    return videoTimebase;
}

void analyzeYUVData(uint8_t *y, uint8_t *u, uint8_t *v, int rowStride, int pixelStride) {
    int offset = 0;
    int length = yuvWidth;
    memset(videoFrame->data[0], 0, yuvHeight * yuvWidth);
    memset(videoFrame->data[1], 0, yuvWidth >> 1);
    memset(videoFrame->data[2], 0, yuvWidth >> 1);

    for (int row = 0; row < yuvHeight; row++) {
        memcpy((videoFrame->data[0] + length), y, yuvWidth);
        length += yuvWidth;
        y += rowStride;
        if (row < yuvHeight >> 1) {
            for (int col = 0; col < yuvWidth >> 1; col++) {
                *(videoFrame->data[1] + offset) = *(u + col * pixelStride);
                *(videoFrame->data[2] + offset++) = *(v + col * pixelStride);
            }
            u += rowStride;
            v += rowStride;
        }
    }
}

int encodeYUV(int64_t *videoPts) {
    videoFrame->pts = ++(*videoPts);
    av_init_packet(&avVideoPacket);
    avVideoPacket.data = NULL;
    avVideoPacket.size = 0;
    avVideoPacket.flags |= AV_PKT_FLAG_KEY;
    ret = avcodec_send_frame(videoCodecCxt, videoFrame);
    if (ret < 0) {
        LOGError("avcodec_send_video_frame error:%d, %s", ret);
        return ret;
    }
    int encGotFrame = avcodec_receive_packet(videoCodecCxt, &avVideoPacket);
    if(encGotFrame != 0)
        av_packet_unref(&avVideoPacket);
    return encGotFrame;
}

void writeVideoFrame(AVFormatContext *outFormatCxt, int videoStreamId, int64_t startTime) {
    avVideoPacket.stream_index = videoStreamId;
//    avVideoPacket->pts = av_rescale(avVideoPacket->pts, outFormatCxt->streams[0]->time_base.den,
//                                    20);
//    avVideoPacket->dts = av_rescale(avVideoPacket->dts, outFormatCxt->streams[0]->time_base.den,
//                                    20);
    avVideoPacket.pts = avVideoPacket.dts = videoFrame->pts;

    avVideoPacket.pos = -1;
    av_packet_rescale_ts(&avVideoPacket, videoCodecCxt->time_base,
                         outFormatCxt->streams[videoStreamId]->time_base);
    if(firstDts <= 0){
        firstDts = avVideoPacket.dts;
    }
    avVideoPacket.duration =
            (AV_TIME_BASE) * av_q2d(videoTimebase) /
            outFormatCxt->streams[videoStreamId]->time_base.den;

    int64_t ptsTime = av_rescale_q(avVideoPacket.dts,
                                   outFormatCxt->streams[videoStreamId]->time_base,
                                   {1, AV_TIME_BASE});
    if(!firstTime){
        firstTime = (avVideoPacket.dts + startTime);
    }
    int64_t nowTime = av_gettime() - firstTime;
    if (ptsTime > nowTime) {
        LOGE("video sleeptime: %lld", ptsTime - nowTime);
        av_usleep(ptsTime - nowTime);
    }
    ret = av_interleaved_write_frame(outFormatCxt, &avVideoPacket);
    if (ret < 0) {
        LOGError("write video frame ret: %d, %s", ret);
    }

    av_packet_unref(&avVideoPacket);
}

void freeVideoReference(){
    if (videoCodecCxt != NULL) {
        avcodec_close(videoCodecCxt);
        avcodec_free_context(&videoCodecCxt);
        videoCodecCxt = NULL;
    }
    if (videoFrame != NULL) {
        av_frame_free(&videoFrame);
        videoFrame = NULL;
    }
}


#endif