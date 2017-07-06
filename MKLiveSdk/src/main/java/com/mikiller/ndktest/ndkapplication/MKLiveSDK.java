package com.mikiller.ndktest.ndkapplication;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.media.AudioFormat;
import android.media.Image;
import android.media.ImageReader;
import android.os.Build;
import android.os.Handler;
import android.support.v4.app.ActivityCompat;
import android.support.v4.util.Pair;
import android.text.TextUtils;
import android.view.SurfaceHolder;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Created by Mikiller on 2017/7/5.
 */

public class MKLiveSDK {
    private static final String TAG = MKLiveSDK.class.getSimpleName();

    public enum VIDEOQUALITY {
        STANDARD(800000, 768, 432), HIGH(1600000, 768, 432), ORIGINAL(3200000, 1280, 720);
        private int bitRate;
        private Pair<Integer, Integer> srcSize;

        VIDEOQUALITY(int rate, int w, int h) {
            bitRate = rate;
            srcSize = new Pair<>(w, h);
        }

        public int getBitRate() {
            return bitRate;
        }

        public int getWidth() {
            return srcSize.first;
        }

        public int getHeight() {
            return srcSize.second;
        }
    }
    public static final int VIDEO_STANDARD = 1, VIDEO_HIGH = 2, VIDEO_ORIGINAL = 3;
    public static final int AUDIO_STANDARD = 64000, AUDIO_HIGH = 128000, AUDIO_ORIGINAL = 320000;
    public static final int FRONT_CAMERA = 1, BACK_CAMERA = 0;
    public static final int LANDSCAPE = 0, PORTRAIT = 270;

    public static final String URL_STR = "outputUrl", VIDEO_QUALITY = "videoquality", AUDIO_QULITY = "audioquality", ORIENTATION = "orientation", CAMERA_ID = "cameraId";
    private final boolean newApi;
    public static final int NETWORK_ERR = -103;
    private static Context mContext;
    private CameraUtils cameraUtils;
    private OldCameraUtils oldCameraUtils;
    private AudioUtils audioUtils;

    private String outputUrl;
    private int cameraId = FRONT_CAMERA, orientation = PORTRAIT, audioBitRate = AUDIO_STANDARD;
    VIDEOQUALITY quality;

    int channelConfig = AudioFormat.CHANNEL_IN_STEREO; //立体声 声道
    int audioFormat = AudioFormat.ENCODING_PCM_16BIT;

    boolean isStart = false;
    ExecutorService executorService;
    LiveRunnable liveRunnable;
    EncodeCallback encodeCallback;

    private MKLiveSDK() {
        newApi = Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP;
    }

    private static class CREATOR {
        private static MKLiveSDK instance = new MKLiveSDK();
    }

    public static MKLiveSDK getInstance(Context context) {
        mContext = context;
        return CREATOR.instance;
    }

    public static MKLiveSDK getInstance() {
        return CREATOR.instance;
    }

    public void setLiveArgs(String url, int videoQuality, int cameraId, int orientation, int audioQuality) {
        outputUrl = url;
        this.cameraId = cameraId;
        this.orientation = cameraId == 1 ? orientation : (360 - orientation) % 360;
        audioBitRate = audioQuality;
        switch (videoQuality) {
            case VIDEO_STANDARD:
                quality = MKLiveSDK.VIDEOQUALITY.STANDARD;
                break;
            case VIDEO_HIGH:
                quality = MKLiveSDK.VIDEOQUALITY.HIGH;
                break;
            case VIDEO_ORIGINAL:
                quality = MKLiveSDK.VIDEOQUALITY.ORIGINAL;
                break;
        }

    }

    public void callLiveActivity(Context act, Intent intent) throws IllegalAccessException {
        if (act == null) {
            throw new IllegalArgumentException("act cannot be null!");
        }
        if (TextUtils.isEmpty(outputUrl))
            throw new IllegalArgumentException("sdk must be set live args first!");
        if (intent == null)
            intent = new Intent(act, MKLiveActivity.class);
        act.startActivity(intent);
    }

    public void setOrientation(Activity act) {
        act.setRequestedOrientation(orientation == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE ? ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE : ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
    }

    public int getPreViewWidth() {
        return quality.getWidth();
    }

    public int getPreViewHeight() {
        return quality.getHeight();
    }

    public void init(EncodeCallback encodeCallback, SurfaceHolder... extSurfaceHolders) {
        initFFMpeg();
        initCamera(encodeCallback, extSurfaceHolders);
        initAudioRecord();
        executorService = Executors.newFixedThreadPool(2);
        executorService.execute(liveRunnable = new LiveRunnable());
    }

    public void initFFMpeg() {
        NDKImpl.initFFMpeg(outputUrl, orientation, quality.getWidth(), quality.getHeight(), channelConfig == AudioFormat.CHANNEL_IN_MONO ? 1 : 2, quality.getBitRate(), audioBitRate);
    }

    @SuppressLint("NewApi")
    private void initCamera(EncodeCallback encodeCallback, final SurfaceHolder... extSurfaceHolders) {
        if (ActivityCompat.checkSelfPermission(mContext, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            // TODO: Consider calling
            encodeCallback.onPermissionFailed(Manifest.permission.CAMERA);
            return;
        }
        if (newApi) {
            cameraUtils = CameraUtils.getInstance(mContext);
            cameraUtils.init(quality.getWidth(), quality.getHeight(), ImageFormat.YUV_420_888, extSurfaceHolders);
            cameraUtils.setPreviewCallback(new ImageReader.OnImageAvailableListener() {

                @Override
                public void onImageAvailable(ImageReader reader) {
                    Image img = reader.acquireNextImage();
                    liveRunnable.setParams(cameraUtils.getNV21Buffer(img));
                    img.close();
                }
            });
        } else {
            oldCameraUtils = OldCameraUtils.getInstance();
            new Thread(new Runnable() {
                @Override
                public void run() {
            oldCameraUtils.initCamera(extSurfaceHolders[0], quality.getWidth(), quality.getHeight(), cameraId, cameraId == 1 ? (360 - orientation)%360 : orientation );
                    oldCameraUtils.setPreviewCallback(new Camera.PreviewCallback() {
                        @Override
                        public void onPreviewFrame(byte[] data, Camera camera) {
                            camera.addCallbackBuffer(data);
                            liveRunnable.setParams(data);
                        }
                    });
                }
            }).start();

        }
        this.encodeCallback = encodeCallback;
    }

    private void initAudioRecord() {
        if (ActivityCompat.checkSelfPermission(mContext, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            encodeCallback.onPermissionFailed(Manifest.permission.RECORD_AUDIO);
            return;
        }
        audioUtils = AudioUtils.getInstance();
        audioUtils.init(channelConfig, audioFormat);
    }

    public void openCamera() {
        if (newApi) {
            cameraUtils.openCamera(String.valueOf(cameraId));
        }
    }

    public void switchCamera() {
        if (newApi) {
            pause();
            NDKImpl.setRotate(cameraUtils.switchCamera());
            new Handler().postDelayed(new Runnable() {
                @Override
                public void run() {
                    start();
                }
            }, 350);
        } else {
            final int rotate = oldCameraUtils.switchCamera();
            new Handler().postDelayed(new Runnable() {
                @Override
                public void run() {
                    NDKImpl.setRotate(rotate);
                }
            }, 150);

        }
    }

    public void start() {
        if(!isStart) {
            isStart = true;
            NDKImpl.initTS();
        }
        audioUtils.start();
        liveRunnable.isPause = false;
    }

    public void pause() {
        audioUtils.pause();
        liveRunnable.isPause = true;
    }

    public void onDestory() {
        NDKImpl.flush();
        liveRunnable.isLive = false;
        isStart = false;
        if (newApi)
            cameraUtils.release();
        else
            oldCameraUtils.release();
        audioUtils.release();
        NDKImpl.close();
    }

    private class LiveRunnable implements Runnable {
        public boolean isLive = true, isPause = true;
        byte[] nv21;
        int ret;

        public void setParams(byte[] nv21) {
            this.nv21 = nv21;
        }

        @Override
        public void run() {
            while (isLive) {
                if (newApi) {
                }
                if (isStart) {
                    if (nv21 != null) {
                        ret = NDKImpl.pushVideo(nv21, isPause);
                        if (ret < 0 && ret != -11) {
                            isStart = false;
                            encodeCallback.onEncodeFailed(ret);
                        }
                    }
                    ret = NDKImpl.pushAudio(audioUtils.getAudioData());
                    if (ret < 0 && ret != -11) {
                        isStart = false;
                        encodeCallback.onEncodeFailed(ret);
                    }
                }
            }
            isLive = true;
            isPause = true;
        }
    }

    public interface EncodeCallback {
        void onPermissionFailed(String permission);

        void onEncodeFailed(int error);
    }
}
