package com.mikiller.ndktest.ndkapplication;

/**
 * Created by Mikiller on 2016/11/24.
 */

public class NDKImpl {
    public native String helloWorld(String input);

    public static native int pushRTMP(String input, String output);

    public static native int initFFMpeg(String outputUrl, int width, int height);

    public static native int encodeYUV(byte[] yuvData);

    public static native int flush();

    public static native int close();
}