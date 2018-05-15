package com.mikiller.ndk.mklivesdk;

import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.util.Log;
import android.view.SurfaceHolder;

import java.util.List;

/**
 * Created by Mikiller on 2017/6/30.
 */

public class OldCameraUtils {
    private static final String TAG = OldCameraUtils.class.getSimpleName();

    private Camera mCamera = null;
    private SurfaceHolder holder;
    private int mVideoWidth, mVideoHeight;
    private int cameraId, orientation;
    Camera.PreviewCallback mCallback;

    private OldCameraUtils() {

    }

    private static class OldCameraFactory {
        private static OldCameraUtils instance = new OldCameraUtils();
    }

    public static OldCameraUtils getInstance() {
        return OldCameraFactory.instance;
    }

    public void initCamera(final SurfaceHolder h, final int width, final int height, final int Id, final int ori) {
        holder = h;
        mVideoWidth = width;
        mVideoHeight = height;
        cameraId = Id;
        orientation = ori;

        mCamera = Camera.open(cameraId);

        mCamera.setDisplayOrientation(orientation);
        Camera.Parameters parameters = mCamera.getParameters();
        List<String> focusModesList = parameters.getSupportedFocusModes();
        for (int i = 0; i < focusModesList.size(); i++) {
            Log.e(TAG, focusModesList.get(i).toString());
        }
        if (focusModesList.contains(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE)) {
            parameters.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE);
        } else if (focusModesList.contains(Camera.Parameters.FOCUS_MODE_AUTO)) {
            parameters.setFocusMode(Camera.Parameters.FOCUS_MODE_AUTO);
        } else if (focusModesList.contains(Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO)) {
            parameters.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO);
        }

        parameters.setPreviewSize(width, height);

        List<int[]> fpsRange = parameters.getSupportedPreviewFpsRange();
        for (int i = 0; i < fpsRange.size(); i++) {
            int[] r = fpsRange.get(i);
            if (r[0] >= 25000 && r[1] >= 25000) {
                parameters.setPreviewFpsRange(r[0], r[1]);
                break;
            }
        }

        parameters.setPreviewFormat(ImageFormat.NV21);

        try {
            mCamera.setParameters(parameters);
        } catch (Exception e) {
        }

        Camera.Size captureSize = mCamera.getParameters().getPreviewSize();

        mCamera.addCallbackBuffer(new byte[captureSize.width * captureSize.height * ImageFormat.getBitsPerPixel(ImageFormat.NV21)]);

        try {
            mCamera.setPreviewDisplay(holder);
        } catch (Exception ex) {
        }
    }

    public void setPreviewCallback(Camera.PreviewCallback callback) {
        if (callback != null) {
            mCallback = callback;
            mCamera.setPreviewCallbackWithBuffer(callback);
        }
        mCamera.startPreview();
    }

    public int switchCamera() {
        mCamera.stopPreview();
        mCamera.release();
        cameraId = (cameraId + 1) % 2;
        initCamera(holder, mVideoWidth, mVideoHeight, cameraId, orientation);
        setPreviewCallback(mCallback);
        return cameraId == 0 ? orientation : (360 - orientation) % 360;
    }

    public void switchFlash(boolean isOpen){
        Camera.Parameters parameters = mCamera.getParameters();
        if(parameters.getSupportedFlashModes() == null)
            return;
        parameters.setFlashMode(isOpen ? Camera.Parameters.FLASH_MODE_TORCH : Camera.Parameters.FLASH_MODE_OFF);
        mCamera.setParameters(parameters);
        mCamera.startPreview();
    }

    public void release() {
        if(mCamera != null)
            mCamera.release();
        holder = null;
    }
}
