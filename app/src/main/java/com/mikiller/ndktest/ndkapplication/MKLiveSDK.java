package com.mikiller.ndktest.ndkapplication;

import android.content.Context;
import android.hardware.Camera;
import android.support.v4.util.Pair;
import android.util.DisplayMetrics;
import android.util.Size;

/**
 * Created by Mikiller on 2017/7/5.
 */

public class MKLiveSDK {
    public enum VIDEOQUALITY{
        STANDARD(800000, 640, 480), HIGH(1600000, 768, 432), ORIGINAL(3200000, 1280, 720);
        private int bitRate;
        private Pair<Integer, Integer> srcSize;
        VIDEOQUALITY(int rate, int w, int h){
            bitRate = rate;
            srcSize = new Pair<>(w, h);
        }

        public int getBitRate(){
            return bitRate;
        }

        public Pair getPreviewSize(){
            return srcSize;
        }
    }
    private static Context mContext;
    private CameraUtils cameraUtils;
    private OldCameraUtils oldCameraUtils;
    private AudioUtils audioUtils;

    private String outputUrl;
    private int cameraId, orientation;

    private MKLiveSDK(){}

    private static class CREATOR{
        private static MKLiveSDK instance = new MKLiveSDK();
    }

    public static MKLiveSDK getInstance(Context context){
        mContext = context;
        return CREATOR.instance;
    }

    public static MKLiveSDK getInstance(){
        return CREATOR.instance;
    }

    public void init(){

    }
}
