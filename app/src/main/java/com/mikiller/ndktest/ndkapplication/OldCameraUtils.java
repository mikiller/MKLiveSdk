package com.mikiller.ndktest.ndkapplication;

import android.content.Context;
import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.view.SurfaceHolder;

import java.util.List;

/**
 * Created by Mikiller on 2017/6/30.
 */

public class OldCameraUtils {
    private Camera mCamera = null;
    private static Context mContext = null;

    private int mVideoWidth, mVideoHeight;
    private SurfaceHolder holder;
    private int mBufSize;
    private boolean isLiving = false;

    private OldCameraUtils(){

    }

    private static class OldCameraFactory{
        private static OldCameraUtils instance = new OldCameraUtils();
    }

    public static OldCameraUtils getInstance(Context context){
        mContext = context;
        return OldCameraFactory.instance;
    }

    public static OldCameraUtils getInstance(){
        return OldCameraFactory.instance;
    }

    public void initCamera(SurfaceHolder holder, int width, int height){
        this.holder = holder;
        this.mVideoWidth = width;
        this.mVideoHeight = height;
        mCamera = Camera.open();
        mCamera.autoFocus(null);
        mCamera.setDisplayOrientation(getDisplayOrientation(0, 0));
        Camera.Parameters parameters = mCamera.getParameters();
        List<String> focusModesList = parameters.getSupportedFocusModes();
        for(int i=0;i<focusModesList.size();i++)
        {
        }
        if (focusModesList.contains(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE)) {
            parameters.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE);
        }else if (focusModesList.contains(Camera.Parameters.FOCUS_MODE_AUTO)) {
            parameters.setFocusMode(Camera.Parameters.FOCUS_MODE_AUTO);
        }else if (focusModesList.contains(Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO)) {
            parameters.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_VIDEO);
        }


        List previewSizes = this.mCamera.getParameters().getSupportedPreviewSizes();
        for (int i = 0; i < previewSizes.size(); i++) {
            Camera.Size s = (Camera.Size)previewSizes.get(i);

            if ((s.width == this.mVideoWidth) && (s.height == this.mVideoHeight)) {
                this.mVideoWidth = s.width;
                this.mVideoHeight = s.height;
                parameters.setPreviewSize(s.width, s.height);
                break;
            }
        }

//        if (flash_mode == 1) {
//            parameters.setFlashMode(Camera.Parameters.FLASH_MODE_TORCH);
//        } else {
//            parameters.setFlashMode(Camera.Parameters.FLASH_MODE_OFF);
//        }

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

        mBufSize = captureSize.width * captureSize.height * ImageFormat.getBitsPerPixel(ImageFormat.NV21) / 8;
        for (int i = 0; i < 3; i++) {
            byte[] buffer = new byte[mBufSize];
            mCamera.addCallbackBuffer(buffer);
        }

        mCamera.setPreviewCallbackWithBuffer(new Camera.PreviewCallback() {
            @Override
            public void onPreviewFrame(byte[] data, Camera camera) {
                if (data == null) {
                    mBufSize += mBufSize / 20;
                    camera.addCallbackBuffer(new byte[mBufSize]);
                } else {
                    camera.addCallbackBuffer(data);
                    if(isLiving)
                        NDKImpl.pushVideo(data, false);

//                    if (mStreamPushProcessor.GetMediaLiveStatus() == 1 && mStreamPushProcessor != null) {
//
//                        if (mEnablevideo==1) {
//                            mStreamPushProcessor.OnCaptureVideoFrame(data, this.mVideoWidth, this.mVideoHeight, 0L, getCameraFace(), mCameraOrientation);
//                        }
//                    }
                }
            }
        });
        try {
            mCamera.setPreviewDisplay(holder);
        } catch (Exception ex) {
        }

        mCamera.startPreview();
        //mPreviewRunning = true;

//        if (sRotateModel.contains(Build.MODEL)) {
//            mInputCameraFace = Camera.CameraInfo.CAMERA_FACING_BACK;
//        }
    }

    public void start(){
        isLiving = true;
    }

    public void pause(){
        isLiving = false;
    }

//    public Size getPreviewSize(Size defaultSize) {
//        Size previewSize = defaultSize;
//        try {
//            for (String id : cameraManager.getCameraIdList()) {
//                if (id.equals(String.valueOf(CameraCharacteristics.LENS_FACING_FRONT))) {
//                    CameraCharacteristics cc = cameraManager.getCameraCharacteristics(id);
//                    StreamConfigurationMap configurationMap = cc.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
//                    previewSize = getMaxSize(configurationMap.getOutputSizes(ImageFormat.YUV_420_888), defaultSize); // or use custom size
//                    break;
//                }
//            }
//        } catch (CameraAccessException e) {
//            e.printStackTrace();
//        } finally {
//            return previewSize;
//        }
//
//    }
//
//    private Size getMaxSize(Size[] sizes, Size defaultSize) {
//        Size maxSize = defaultSize;
//        //if (sizes == null)
////            return maxSize;
////        Collections.sort(Arrays.asList(sizes), new Comparator<Size>() {
////            @Override
////            public int compare(Size lhs, Size rhs) {
////                if (rhs.getWidth() - lhs.getWidth() == 0)
////                    return rhs.getHeight() - lhs.getHeight();
////                return rhs.getWidth() - lhs.getWidth();
////            }
////        });
//
//        for (Size size : sizes) {
//            Log.e(TAG, "w: " + size.getWidth() + " h: " + size.getHeight());
////            if (size.getWidth() - defaultSize.getWidth() <= 0) {
////                if (size.getHeight() - defaultSize.getHeight() <= 0) {
////                    maxSize = size;
//////                    maxSize = new Size(size.getHeight(), size.getWidth());
////                    break;
////                }
////            }
//        }
//        //support size : 1280*720, 768*432, 640*480
//        return maxSize;
//    }

    public static int getDisplayOrientation(int degrees, int cameraId) {

        Camera.CameraInfo info = new Camera.CameraInfo();
        Camera.getCameraInfo(cameraId, info);
        int result;
        if (info.facing == Camera.CameraInfo.CAMERA_FACING_FRONT) {
            result = (info.orientation + degrees) % 360;
            result = (360 - result) % 360;
        } else {
            result = (info.orientation - degrees + 360) % 360;
        }
        return result;
    }
}
