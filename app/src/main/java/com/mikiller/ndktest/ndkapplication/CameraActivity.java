package com.mikiller.ndktest.ndkapplication;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.ImageReader;
import android.os.Bundle;
import android.os.Handler;
import android.support.v4.app.ActivityCompat;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.Size;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.TextureView;
import android.widget.CheckBox;
import android.widget.ImageButton;

import butterknife.BindView;
import butterknife.ButterKnife;
import butterknife.Unbinder;

/**
 * Created by Mikiller on 2016/12/2.
 */

public class CameraActivity extends Activity implements SurfaceHolder.Callback {
    Unbinder unbinder;

    @BindView(R.id.surfaceViewEx)
    SurfaceView surfaceViewEx;
    @BindView(R.id.SwitchCamerabutton)
    ImageButton SwitchCamerabutton;
    @BindView(R.id.FlashCamerabutton)
    ImageButton FlashCamerabutton;
    @BindView(R.id.ckb_play)
    CheckBox ckb_play;

    String outputUrl;
    int width, height;
    Size previewSize;
    CameraManager cameraManager;
    @SuppressLint("NewApi")
    CameraDevice.StateCallback cameraCallback = new CameraDevice.StateCallback() {
        @Override
        public void onOpened(CameraDevice camera) {

        }

        @Override
        public void onDisconnected(CameraDevice camera) {

        }

        @Override
        public void onError(CameraDevice camera, int error) {

        }
    };
//    ImageReader imageReader;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        outputUrl = getIntent().getStringExtra("outputUrl");
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_camera);
        unbinder = ButterKnife.bind(this);

        initFFMpeg();
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {

    }

    @Override
    protected void onResume() {
        super.onResume();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        initCamera(width, height);
    }

    @SuppressLint("NewApi")
    private void initCamera(int width, int height) {
        cameraManager = (CameraManager) getSystemService(Context.CAMERA_SERVICE);
        try {
            String[] cameraIds = cameraManager.getCameraIdList();
            if (cameraIds.length <= 0)
                return;
            CameraCharacteristics cc = cameraManager.getCameraCharacteristics(cameraIds[0]);
            StreamConfigurationMap configurationMap = cc.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
            Size[] outputSize = configurationMap.getOutputSizes(SurfaceTexture.class);
            previewSize = getMaxSize(outputSize); // or use custom size
            if (previewSize == null)
                return;
//            imageReader = ImageReader.newInstance(previewSize.getWidth(), previewSize.getHeight(), )
            if (ActivityCompat.checkSelfPermission(this, Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
                // TODO: Consider calling
                //    ActivityCompat#requestPermissions
                // here to request the missing permissions, and then overriding
                //   public void onRequestPermissionsResult(int requestCode, String[] permissions,
                //                                          int[] grantResults)
                // to handle the case where the user grants the permission. See the documentation
                // for ActivityCompat#requestPermissions for more details.
                return;
            }
            cameraManager.openCamera(cameraIds[0], cameraCallback, new Handler());
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }

    }

    @SuppressLint("NewApi")
    private Size getMaxSize(Size[] sizes){
        Size maxSize = null;
        if(sizes == null)
            return maxSize;
        maxSize = sizes[0];
        for(Size size : sizes){
            if(size.getWidth() * size.getHeight() > maxSize.getWidth() * maxSize.getHeight()) {
                Log.e(CameraActivity.class.getSimpleName(), "width: " + size.getWidth() + " height: " + size.getHeight());
                maxSize = size;
            }
        }
        return maxSize;
    }

    private void initFFMpeg() {
        DisplayMetrics dm = getResources().getDisplayMetrics();
        width = dm.widthPixels;
        height = dm.heightPixels;
        //NDKImpl.initFFMpeg(outputUrl, width, height);
    }


    @Override
    protected void onPause() {
        super.onPause();
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {

    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        unbinder.unbind();
    }

}
