package com.mikiller.ndktest.ndkapplication;

import android.app.Activity;
import android.content.Context;
import android.hardware.camera2.CameraManager;
import android.os.Bundle;
import android.util.DisplayMetrics;
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
    CameraManager cameraManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        outputUrl = getIntent().getStringExtra("outputUrl");
        super.onCreate(savedInstanceState);
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

    private void initCamera(int width, int height){
        cameraManager = (CameraManager) getSystemService(Context.CAMERA_SERVICE);
        
    }

    private void initFFMpeg(){
        DisplayMetrics dm = getResources().getDisplayMetrics();
        width = dm.widthPixels;
        height = dm.heightPixels;
        NDKImpl.initFFMpeg(outputUrl, width, height);
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
