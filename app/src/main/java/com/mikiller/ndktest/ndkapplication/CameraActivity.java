package com.mikiller.ndktest.ndkapplication;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.graphics.ImageFormat;
import android.hardware.camera2.CameraCharacteristics;
import android.media.AudioFormat;
import android.media.Image;
import android.media.ImageReader;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.RequiresApi;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.util.Size;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.ImageButton;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import butterknife.BindView;
import butterknife.ButterKnife;
import butterknife.Unbinder;

/**
 * Created by Mikiller on 2016/12/2.
 */
@SuppressLint("NewApi")
public class CameraActivity extends AppCompatActivity {
    private static final String TAG = CameraActivity.class.getSimpleName();
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

    CameraUtils cameraUtils;
    OldCameraUtils oldCameraUtils;
    AudioUtils audioUtils;
    int orientation;
    CameraUtils.VIDEOQUALITY quality;
    int audioBitRate;

    int channelConfig = AudioFormat.CHANNEL_IN_STEREO; //立体声 声道
    int audioFormat = AudioFormat.ENCODING_PCM_16BIT;


    boolean isNetWorkError = false;
    ExecutorService executorService;
    private boolean isPlay = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        outputUrl = getIntent().getStringExtra("outputUrl");
        switch (getIntent().getIntExtra("videoQuality", 1)){
            case 1:
                quality = CameraUtils.VIDEOQUALITY.STANDARD;
                break;
            case 2:
                quality = CameraUtils.VIDEOQUALITY.HIGH;
                break;
            case 3:
                quality = CameraUtils.VIDEOQUALITY.ORIGINAL;
                break;
        }
        audioBitRate = getIntent().getIntExtra("audioQuality", 64000);
        orientation = getIntent().getIntExtra("orientation",90);
        setRequestedOrientation(orientation == ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE ? ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE : ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);

        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_camera);
        unbinder = ButterKnife.bind(this);

        initFFMpeg(quality.getPreviewSize());
        initCamera();
        initAudioRecord();
        initView(quality.getPreviewSize());
        executorService = Executors.newFixedThreadPool(2);
        executorService.execute(cameraUtils.getVideoRunnable());
        executorService.execute(audioUtils.getAudioRunnable());
    }

    private void initCamera(){
//        oldCameraUtils = OldCameraUtils.getInstance(this);

        cameraUtils = CameraUtils.getInstance(this);
        cameraUtils.init(quality.getPreviewSize(), ImageFormat.YUV_420_888, surfaceViewEx.getHolder().getSurface());
        cameraUtils.setEncodeCallback(new CameraUtils.EncodeCallback() {
            @Override
            public void onEncodeFailed(final int error) {
                    isPlay = false;
                    ckb_play.post(new Runnable() {
                        @Override
                        public void run() {
                            ckb_play.setChecked(isPlay);
                            if (error == -103)
                                isNetWorkError = true;
                        }
                    });
            }
        });
    }

    private void initAudioRecord() {
        audioUtils = AudioUtils.getInstance();
        audioUtils.init(channelConfig, audioFormat);
    }

    private void initView(final Size previewSize) {
        surfaceViewEx.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                Log.e(TAG, "surface created");
                holder.setFixedSize(previewSize.getWidth(), previewSize.getHeight());
                holder.setKeepScreenOn(true);
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                if(width == previewSize.getWidth()) {
                    cameraUtils.openCamera(String.valueOf(CameraCharacteristics.LENS_FACING_FRONT));
//                        new Thread(new Runnable() {
//                            @Override
//                            public void run() {
//                                oldCameraUtils.initCamera(surfaceViewEx.getHolder(), previewSize.getWidth(), previewSize.getHeight());
//                            }
//                        }).start();
                }
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                Log.e(TAG, "surface destoryed");
                cameraUtils.release();
            }
        });

        ckb_play.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                isPlay = isChecked;
                if (!isPlay) {
                    Log.e(TAG, "pause video thread");
                    cameraUtils.pause();
                    audioUtils.pause();
//                    oldCameraUtils.pause();
                } else{
                    if (isNetWorkError) {
                        initFFMpeg(quality.getPreviewSize());
                        isNetWorkError = false;
                    }else{
                        Log.e(TAG, "start video thread");
                        NDKImpl.initTS();
                        cameraUtils.start();
                        audioUtils.start();
//                        oldCameraUtils.start();
//                        audioUtils.audioSwitch(isPlay);
                    }
                }
//                audioUtils.audioSwitch(isPlay);
            }
        });

        SwitchCamerabutton.setOnClickListener(new View.OnClickListener() {
            @RequiresApi(api = Build.VERSION_CODES.LOLLIPOP)
            @Override
            public void onClick(View v) {
                cameraUtils.switchCamera();
            }
        });

    }

    private void initFFMpeg(Size size) {
        NDKImpl.initFFMpeg(outputUrl, orientation, size.getWidth(), size.getHeight(), channelConfig == AudioFormat.CHANNEL_IN_MONO ? 1 : 2, quality.getBitRate(), audioBitRate);
    }

    @Override
    protected void onResume() {
        super.onResume();
//        ckb_play.setChecked(true);
    }

    @Override
    protected void onPause() {
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.e(TAG, "camera activity destoryed");
        NDKImpl.flush();
        Log.e(TAG, "ffmpeg flush");
        NDKImpl.close();
        Log.e(TAG, "ffmepg close");
        cameraUtils.release();
        audioUtils.release();
        unbinder.unbind();

    }




}
