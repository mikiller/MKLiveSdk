package com.mikiller.ndktest.ndkapplication;

/**
 * Created by Mikiller on 2016/11/24.
 */

public class NDKImpl {

    public static native int initFFMpeg(String outputUrl, int orientation, int width, int height, int channels, int videoBitRate, int audioBitRate);

    public static native int pushAudio(byte[] bytes);

    public static native int pushVideo(byte[] bytes, boolean isPause);

    public static native void initTS();

    public static native void setRotate(int rotate);

    public static native int flush();

    public static native int close();
}
