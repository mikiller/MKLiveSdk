package com.mikiller.ndktest.ndkapplication;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.Image;
import android.media.ImageReader;
import android.os.Handler;
import android.os.HandlerThread;
import android.support.v4.app.ActivityCompat;
import android.util.Log;
import android.util.Size;
import android.util.SparseIntArray;
import android.view.Surface;

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
    private static final SparseIntArray ORIENTATIONS = new SparseIntArray(); ///为了使照片竖直显示
    static {
        ORIENTATIONS.append(Surface.ROTATION_0, 90);
        ORIENTATIONS.append(Surface.ROTATION_90, 0);
        ORIENTATIONS.append(Surface.ROTATION_180, 270);
        ORIENTATIONS.append(Surface.ROTATION_270, 180);
    }
    static Context mContext;
    static CameraManager cameraManager;
    CameraDevice cameraDevice;
    ImageReader imageReader;
    Handler handler;
    CameraDevice.StateCallback cameraCallback;
    CameraCaptureSession.StateCallback stateCallback;
    Size previewSize;
    List<Surface> surfaceList = new ArrayList<>();
    StreamTask streamTask = new StreamTask();
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

    public StreamTask getStreamTask(){
        return streamTask;
    }

    public EncodeCallback getEncodeCallback() {
        return encodeCallback;
    }

    public void setEncodeCallback(EncodeCallback encodeCallback) {
        this.encodeCallback = encodeCallback;
    }

    public void init(Size defaultSize, int format, Surface... extSurfaces) {
        HandlerThread thread = new HandlerThread("camera2");
        thread.start();
        handler = new Handler(thread.getLooper());
        previewSize = getPreviewSize(defaultSize);

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
                cameraDevice = camera;
                startPreview(camera);
            }

            @Override
            public void onDisconnected(CameraDevice camera) {
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

        stateCallback = new CameraCaptureSession.StateCallback() {

            @Override
            public void onConfigured(CameraCaptureSession session) {
                try {
                    CaptureRequest.Builder previewBuild = cameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_RECORD);
                    for (Surface surface : surfaceList) {
                        previewBuild.addTarget(surface);
                    }
                    // 自动对焦
                    previewBuild.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_VIDEO);
                    // 打开闪光灯
                    previewBuild.set(CaptureRequest.CONTROL_AE_MODE, CaptureRequest.CONTROL_AE_MODE_ON_AUTO_FLASH);

                    int rotation = ((Activity)mContext).getWindowManager().getDefaultDisplay().getRotation();
                    previewBuild.set(CaptureRequest.JPEG_ORIENTATION, ORIENTATIONS.get(rotation));

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

    public Size getPreviewSize() {
        return previewSize;
    }

    public Size getPreviewSize(Size defaultSize) {
        Size previewSize = defaultSize;
        try {
            for (String id : cameraManager.getCameraIdList()) {
                if (id.equals(String.valueOf(CameraCharacteristics.LENS_FACING_FRONT))) {
                    CameraCharacteristics cc = cameraManager.getCameraCharacteristics(id);
                    StreamConfigurationMap configurationMap = cc.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
                    previewSize = getMaxSize(configurationMap.getOutputSizes(SurfaceTexture.class), defaultSize); // or use custom size
                    break;
                }
            }
        } catch (CameraAccessException e) {
            e.printStackTrace();
        } finally {
            return previewSize;
        }

    }

    private Size getMaxSize(Size[] sizes, Size defaultSize) {
        Size maxSize = defaultSize;
        if (sizes == null)
            return maxSize;
        Collections.sort(Arrays.asList(sizes), new Comparator<Size>() {
            @Override
            public int compare(Size lhs, Size rhs) {
                if (rhs.getWidth() - lhs.getWidth() == 0)
                    return rhs.getHeight() - lhs.getHeight();
                return rhs.getWidth() - lhs.getWidth();
            }
        });

        for (Size size : sizes) {
            Log.e(CameraActivity.class.getSimpleName(), "w: " + size.getWidth() + " h: " + size.getHeight());
            if (size.getWidth() - defaultSize.getWidth() <= 0) {
                if (size.getHeight() - defaultSize.getHeight() <= 0) {
                    maxSize = size;
//                    maxSize = new Size(size.getHeight(), size.getWidth());
                    break;
                }
            }
        }
        return maxSize;
    }

    public void setPreviewCallback(ImageReader.OnImageAvailableListener listener) {
        if (listener != null)
            imageReader.setOnImageAvailableListener(listener, handler);
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

    public void startPreview(CameraDevice cameraDevice) {
        try {
            cameraDevice.createCaptureSession(surfaceList, stateCallback, handler);
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

    public void updateImage(Image image){
        streamTask.setParams(getYUVBuffer(image), image.getPlanes()[1].getRowStride(), image.getPlanes()[1].getPixelStride());
        synchronized (streamTask) {
            streamTask.notify();
        }
    }

    public byte[][] getYUVBuffer(Image image){
        int planeSize = image.getPlanes().length;
        ByteBuffer[] yuvBuffer = new ByteBuffer[planeSize];
        byte[][] yuvbytes = new byte[planeSize][];
        for (int i = 0; i < planeSize; i++) {
            yuvBuffer[i] = image.getPlanes()[i].getBuffer();
            yuvbytes[i] = new byte[yuvBuffer[i].remaining()];
            yuvBuffer[i].get(yuvbytes[i]);
        }
        return yuvbytes;
    }

    public void release() {
        if (cameraDevice != null) {
            cameraDevice.close();
            cameraDevice = null;
        }
        if (cameraDevice != null) {
            cameraDevice.close();
            cameraDevice = null;
        }
        surfaceList.clear();
        handler.getLooper().getThread().interrupt();
        handler.getLooper().quitSafely();
    }

    private class StreamTask implements Runnable {

        byte[][] yuvBytes;
        int rowStride, pixelStride;

        public void setParams(byte[][] yuvBytes, int rowStride, int pixelStride) {
            this.yuvBytes = yuvBytes;
            this.rowStride = rowStride;
            this.pixelStride = pixelStride;
        }

        @Override
        public void run() {
            while (true) {
                synchronized (streamTask) {
                    try {
                        this.wait();
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }
                final int ret = NDKImpl.encodeData(yuvBytes[0], yuvBytes[1], yuvBytes[2], rowStride, pixelStride);
                if (ret < 0 && encodeCallback != null) {
                    encodeCallback.onEncodeFailed(ret);

                }

            }
        }
    }

    public interface EncodeCallback{
        void onEncodeFailed(int error);
    }
}
