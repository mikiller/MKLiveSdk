package com.mikiller.ndktest.ndkapplication;

/**
 * Created by Mikiller on 2016/11/24.
 */

public class NDKImpl {
    public native String helloWorld(String input);

    public static native int pushRTMP(String input, String output);

    public static native int initFFMpeg(String outputUrl, int width, int height, int channels, int videoBitRate, int audioBitRate);

    public static native int saveAudioBuffer(byte[] bytes, int length);

    public static native int encodeData(byte[] bytes, byte[] bytesU, byte[] bytesV, int rowStride, int pixelStride);

    public static native void initStartTime();

    public static native int flush();

    public static native int close();
}
