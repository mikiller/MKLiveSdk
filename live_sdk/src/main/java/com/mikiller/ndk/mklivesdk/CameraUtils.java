package com.mikiller.ndk.mklivesdk;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.ImageFormat;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraMetadata;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.Image;
import android.media.ImageReader;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.util.Size;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * Created by Mikiller on 2016/12/6.
 */
@SuppressLint("NewApi")
public class CameraUtils {
    private static final String TAG = CameraUtils.class.getSimpleName();

    static CameraManager cameraManager;
    String cameraId;
    CameraDevice cameraDevice;
    CameraCaptureSession cSession;
    CaptureRequest.Builder previewBuild;
    ImageReader imageReader;
    HandlerThread cameraThread;
    Handler handler;
    ImageReader.OnImageAvailableListener previewListener;
    CameraDevice.StateCallback cameraCallback;
    CameraCaptureSession.StateCallback captureCallback;
    List<Surface> surfaceList = new ArrayList<>();
    ByteBuffer nv21;
    boolean hasInit = false;

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
        return VideoUtilsFactory.instance;
    }

    public void init(final int width, final int height, final InitCallback initCallback, final int format, final SurfaceHolder... extSurfaceHolders) {
        cameraThread = new HandlerThread("camera2");
        cameraThread.start();
        handler = new Handler(cameraThread.getLooper());

        cameraCallback = new CameraDevice.StateCallback() {
            @Override
            public void onOpened(CameraDevice camera) {
                Log.e(TAG, "camera opened");
                cameraDevice = camera;

                if(!hasInit) {
                    initCallback.onInitSuccess();
                    hasInit = true;
                }else
                    startPreview();
                nv21 = null;
            }

            @Override
            public void onDisconnected(CameraDevice camera) {
                Log.e(TAG, "camera disconnected");
                camera.close();
                cameraDevice = null;
            }

            @Override
            public void onError(CameraDevice camera, int error) {
                Log.e(MKLiveActivity.class.getSimpleName(), "error:" + error);
                camera.close();
                cameraDevice = null;
            }
        };

        captureCallback = new CameraCaptureSession.StateCallback() {

            @Override
            public void onConfigured(CameraCaptureSession session) {
                try {
                    cSession = session;

                    previewBuild = cameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_RECORD);


                    for (Surface surface : surfaceList) {
                        previewBuild.addTarget(surface);
                    }
                    previewBuild.set(CaptureRequest.CONTROL_MODE, CameraMetadata.CONTROL_MODE_AUTO);
                    // 自动对焦
                    previewBuild.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_VIDEO);
                    // 设置闪光灯
                    previewBuild.set(CaptureRequest.CONTROL_AE_MODE, CaptureRequest.CONTROL_AE_MODE_ON);
                    previewBuild.set(CaptureRequest.FLASH_MODE, CaptureRequest.FLASH_MODE_OFF);
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
    }

    public void setSurfaceList(final int width, final int height, final int format, final SurfaceHolder... extSurfaceHolders){
        surfaceList.clear();
        imageReader = ImageReader.newInstance(width, height, format, 1);
        if (previewListener != null)
            imageReader.setOnImageAvailableListener(previewListener, handler);
        surfaceList.add(imageReader.getSurface());
        if (extSurfaceHolders != null) {
            for (SurfaceHolder holder : extSurfaceHolders) {
                surfaceList.add(holder.getSurface());
            }
        }
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
        previewListener = listener;


    }

    public void openCamera(String cameraId) {
        try {
            this.cameraId = cameraId;
            cameraManager.openCamera(cameraId, cameraCallback, handler);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

    public int switchCamera() {
        int rotation;
        cameraDevice.close();
        if (cameraDevice.getId().equals(String.valueOf(CameraCharacteristics.LENS_FACING_BACK))) {
            cameraId = String.valueOf(CameraCharacteristics.LENS_FACING_FRONT);
            rotation = 90;
        } else {
            cameraId = String.valueOf(CameraCharacteristics.LENS_FACING_BACK);
            rotation = 270;
        }
        openCamera(cameraId);
        return rotation;
    }

    public void switchFlash(boolean isOpen){
        try {
            previewBuild = cameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_RECORD);
            previewBuild.set(CaptureRequest.FLASH_MODE, isOpen ? CaptureRequest.FLASH_MODE_TORCH : CaptureRequest.FLASH_MODE_OFF);
            for(Surface surface : surfaceList){
                previewBuild.addTarget(surface);
            }
            cSession.setRepeatingRequest(previewBuild.build(), null, handler);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

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
        if (cameraDevice != null) {
            cameraDevice.close();
            cameraDevice = null;
        }
        for (Surface surface : surfaceList) {
            surface.release();
        }
        surfaceList.clear();
        cameraThread.quitSafely();
        nv21 = null;
        hasInit = false;
    }

    public interface InitCallback{
        void onInitSuccess();
    }
}
