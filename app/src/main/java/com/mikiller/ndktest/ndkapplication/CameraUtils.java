package com.mikiller.ndktest.ndkapplication;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.PackageManager;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraMetadata;
import android.hardware.camera2.CaptureRequest;
import android.media.Image;
import android.media.ImageReader;
import android.os.Handler;
import android.os.HandlerThread;
import android.support.v4.app.ActivityCompat;
import android.util.Log;
import android.util.Size;
import android.view.Surface;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

/**
 * Created by Mikiller on 2016/12/6.
 */
@SuppressLint("NewApi")
public class CameraUtils {
    private static final String TAG = CameraUtils.class.getSimpleName();

    public static enum VIDEOQUALITY{
        STANDARD(800000, 640, 480), HIGH(1600000, 768, 432), ORIGINAL(3200000, 1280, 720);
        private int bitRate;
        private Size srcSize;
        private VIDEOQUALITY(int rate, int w, int h){
            bitRate = rate;
            srcSize = new Size(w, h);
        }

        public int getBitRate(){
            return bitRate;
        }

        public Size getPreviewSize(){
            return srcSize;
        }

        public int getLandWidth(){
            return srcSize.getWidth();
        }

        public int getPortWidth(){
            return srcSize.getHeight();
        }

        public int getLandHeight(){
            return srcSize.getHeight();
        }

        public int getPortHeight(){
            return srcSize.getWidth();
        }
    }

    static Context mContext;
    static CameraManager cameraManager;
    CameraDevice cameraDevice;
    ImageReader imageReader;
    HandlerThread cameraThread;
    Handler handler;
    CameraDevice.StateCallback cameraCallback;
    CameraCaptureSession.StateCallback captureCallback;
    Size previewSize;
    List<Surface> surfaceList = new ArrayList<>();
    VideoRunnable videoRunnable;
    EncodeCallback encodeCallback;

    private CameraUtils() {
    }

    private static class VideoUtilsFactory {
        private static CameraUtils instance = new CameraUtils();
    }

    public static CameraUtils getInstance() {
        return VideoUtilsFactory.instance;
    }

    public static CameraUtils getInstance(Context context) {
        if (cameraManager == null)
            cameraManager = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
        mContext = context;
        return VideoUtilsFactory.instance;
    }

    public VideoRunnable getVideoRunnable() {
        return videoRunnable;
    }

    public EncodeCallback getEncodeCallback() {
        return encodeCallback;
    }

    public void setEncodeCallback(EncodeCallback encodeCallback) {
        this.encodeCallback = encodeCallback;
    }

    public void init(Size defaultSize, int format, Surface... extSurfaces) {
        cameraThread = new HandlerThread("camera2");
        cameraThread.start();
        handler = new Handler(cameraThread.getLooper());
        //previewSize = getPreviewSize(defaultSize);
        previewSize = defaultSize;
        imageReader = ImageReader.newInstance(previewSize.getWidth(), previewSize.getHeight(), format, 1);
        surfaceList.add(imageReader.getSurface());
        if (extSurfaces != null) {
            for (Surface surface : extSurfaces) {
                surfaceList.add(surface);
            }
        }

        cameraCallback = new CameraDevice.StateCallback() {
            @Override
            public void onOpened(CameraDevice camera) {
                Log.e(TAG, "camera opened");
                cameraDevice = camera;
                startPreview();
            }

            @Override
            public void onDisconnected(CameraDevice camera) {
                Log.e(TAG, "camera disconnected");
                camera.close();
                cameraDevice = null;
            }

            @Override
            public void onError(CameraDevice camera, int error) {
                Log.e(CameraActivity.class.getSimpleName(), "error:" + error);
                camera.close();
                cameraDevice = null;
            }
        };

        captureCallback = new CameraCaptureSession.StateCallback() {

            @Override
            public void onConfigured(CameraCaptureSession session) {
                try {
                    CaptureRequest.Builder previewBuild = cameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_RECORD);
                    for (Surface surface : surfaceList) {
                        previewBuild.addTarget(surface);
                    }
                    previewBuild.set(CaptureRequest.CONTROL_MODE, CameraMetadata.CONTROL_MODE_AUTO);
                    // 自动对焦
                    previewBuild.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_VIDEO);
                    // 打开闪光灯
                    previewBuild.set(CaptureRequest.CONTROL_AE_MODE, CaptureRequest.CONTROL_AE_MODE_ON_AUTO_FLASH);

                    session.setRepeatingRequest(previewBuild.build(), null, handler);
                } catch (CameraAccessException e) {
                    e.printStackTrace();
                }
            }

            @Override
            public void onConfigureFailed(CameraCaptureSession session) {
                session.close();
                release();
            }
        };

        setPreviewCallback(new ImageReader.OnImageAvailableListener() {
            @Override
            public void onImageAvailable(ImageReader reader) {
                updateImage(reader.acquireNextImage());
            }
        });
        videoRunnable = new VideoRunnable();
    }

    public void startPreview() {
        try {
            Log.e(TAG, "start preview and set captureCallback");
            cameraDevice.createCaptureSession(surfaceList, captureCallback, handler);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

    public void setPreviewCallback(ImageReader.OnImageAvailableListener listener) {
        if (listener != null)
            imageReader.setOnImageAvailableListener(listener, handler);
    }

    public void updateImage(Image image) {
        videoRunnable.setParams(getNV21Buffer(image));
        image.close();
    }

    public void openCamera(String cameraId) {
        if (ActivityCompat.checkSelfPermission(mContext, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            // TODO: Consider calling
            return;
        }
        try {
            cameraManager.openCamera(cameraId, cameraCallback, null);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

    public void switchCamera() {
        String cameraId;
        if (cameraDevice.getId().equals(String.valueOf(CameraCharacteristics.LENS_FACING_BACK))) {
            cameraId = String.valueOf(CameraCharacteristics.LENS_FACING_FRONT);

        } else {
            cameraId = String.valueOf(CameraCharacteristics.LENS_FACING_BACK);
        }
        cameraDevice.close();
        openCamera(cameraId);
    }

    public void pause(){
        videoRunnable.isPause = true;
    }

    public void start(){
        videoRunnable.isPause = false;
    }

    ByteBuffer nv21;
    public byte[] getNV21Buffer(Image image){
        ByteBuffer yBuffer = image.getPlanes()[0].getBuffer();
        ByteBuffer uvBuffer = image.getPlanes()[2].getBuffer();
        if(nv21 == null){
            int bufferSize = yBuffer.remaining() + uvBuffer.remaining();
            nv21 = ByteBuffer.allocate(bufferSize);
        }else{
            nv21.clear();
        }
        return nv21.put(yBuffer).put(uvBuffer).array();
    }

    public void release() {
        videoRunnable.isLive = false;
        if (cameraDevice != null) {
            cameraDevice.close();
            cameraDevice = null;
        }
        if (cameraDevice != null) {
            cameraDevice.close();
            cameraDevice = null;
        }
        for (Surface surface : surfaceList) {
            surface.release();
        }
        surfaceList.clear();
        cameraThread.quitSafely();
    }

    private class VideoRunnable implements Runnable {
        public boolean isLive = true, isPause = true;
        byte[] nv21;

        public void setParams(byte[] nv21){
            this.nv21 = nv21;
        }

        @Override
        public void run() {
            while (isLive) {

//                if(!isPause) {
                if(nv21 != null)
                    NDKImpl.pushVideo(nv21, isPause);
//                    if (ret < 0 && encodeCallback != null) {
//                        encodeCallback.onEncodeFailed(ret);
//                    }
//                }

            }
            isLive = true;
            isPause = true;
        }
    }

    public interface EncodeCallback {
        void onEncodeFailed(int error);
    }
}
