package com.mikiller.ndktest.ndkapplication;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.ImageFormat;
import android.hardware.camera2.CameraCharacteristics;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.Image;
import android.media.ImageReader;
import android.media.MediaRecorder;
import android.os.Build;
import android.os.Bundle;
import android.support.annotation.RequiresApi;
import android.util.Log;
import android.util.Size;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.ImageButton;

import java.nio.ByteBuffer;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import butterknife.BindView;
import butterknife.ButterKnife;
import butterknife.Unbinder;

/**
 * Created by Mikiller on 2016/12/2.
 */
@SuppressLint("NewApi")
public class CameraActivity extends Activity {
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
    int videoBitRate;

    AudioUtils audioUtils;
    int sampleRate = 44100;
    int channelConfig = AudioFormat.CHANNEL_IN_MONO; //立体声 声道
    int audioFormat = AudioFormat.ENCODING_PCM_16BIT;
    int audioBitRate;

    boolean isNetWorkError = false;
    ExecutorService executorService;
    private boolean isPlay = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        outputUrl = getIntent().getStringExtra("outputUrl");
        videoBitRate = getIntent().getIntExtra("videoQuality", 800);
        audioBitRate = getIntent().getIntExtra("audioQuality", 64);
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_camera);
        unbinder = ButterKnife.bind(this);

        initCamera();
        initAudioRecord();
        initView(cameraUtils.getPreviewSize());

        executorService = Executors.newFixedThreadPool(2);
        executorService.execute(cameraUtils.getStreamTask());
        executorService.execute(audioUtils.getAudioRunnable());

    }

    private void initCamera(){
        cameraUtils = CameraUtils.getInstance(this);
        cameraUtils.init(new Size(1280, 720), ImageFormat.YUV_420_888, surfaceViewEx.getHolder().getSurface());
        cameraUtils.setPreviewCallback(new ImageReader.OnImageAvailableListener() {
            @Override
            public void onImageAvailable(ImageReader reader) {
                Image image = reader.acquireNextImage();
                if (isPlay) {
//                        Log.e(CameraActivity.class.getSimpleName(), "camera callback: " + System.currentTimeMillis());
                    cameraUtils.updateImage(image);
                }
//
                image.close();
            }
        });
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
        audioUtils.init(sampleRate, channelConfig, audioFormat);
    }

    private void initView(final Size previewSize) {
        initFFMpeg(previewSize);
        surfaceViewEx.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {

            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
                cameraUtils.openCamera(String.valueOf(CameraCharacteristics.LENS_FACING_FRONT));
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                cameraUtils.release();
            }
        });
        surfaceViewEx.getHolder().setFixedSize(previewSize.getWidth(), previewSize.getHeight());

        ckb_play.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                isPlay = isChecked;
                if (!isPlay) {
                    NDKImpl.flush();
                } else if (isPlay && isNetWorkError) {
                    initFFMpeg(previewSize);
                    isNetWorkError = false;
                }

                audioUtils.audioSwitch(isPlay);

                NDKImpl.initStartTime();
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

    private void initFFMpeg(Size previewSize) {
        NDKImpl.initFFMpeg(outputUrl, previewSize.getWidth(), previewSize.getHeight(), channelConfig == AudioFormat.CHANNEL_IN_MONO ? 1 : 2, sampleRate, videoBitRate, audioBitRate);
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
        NDKImpl.flush();
        NDKImpl.close();
        cameraUtils.release();
        audioUtils.release();
        unbinder.unbind();
    }




}
