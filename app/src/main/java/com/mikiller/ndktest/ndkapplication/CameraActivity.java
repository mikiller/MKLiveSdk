package com.mikiller.ndktest.ndkapplication;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.ImageFormat;
import android.graphics.Matrix;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.SurfaceTexture;
import android.graphics.YuvImage;
import android.hardware.Camera;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CaptureFailure;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.TotalCaptureResult;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.AudioDeviceCallback;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.Image;
import android.media.ImageReader;
import android.media.MediaRecorder;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.support.v4.app.ActivityCompat;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.Size;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.TextureView;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.ImageButton;

import java.io.IOException;
import java.lang.reflect.Array;
import java.nio.ByteBuffer;
import java.nio.FloatBuffer;
import java.nio.ShortBuffer;
import java.util.ArrayList;
import java.util.Arrays;

import butterknife.BindView;
import butterknife.ButterKnife;
import butterknife.Unbinder;

import static android.media.AudioRecord.READ_BLOCKING;

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
    CameraDevice cameraDevice;
    CaptureRequest.Builder previewBuild;
    CaptureRequest captureRequest;
    CameraCaptureSession captureSession;

    AudioRecord audioRecord;
    int audioBufSize = 0;
    int sampleRate = 44100;
    int audioSource = MediaRecorder.AudioSource.MIC;
    int channelConfig = AudioFormat.CHANNEL_IN_STEREO; //立体声 声道
    int audioFormat = AudioFormat.ENCODING_PCM_FLOAT;
    int audioBitRate = 320*1000;


    ImageReader imageReader;
    Handler handler;

    @SuppressLint("NewApi")
    CameraDevice.StateCallback cameraCallback = new CameraDevice.StateCallback() {
        @Override
        public void onOpened(CameraDevice camera) {
            cameraDevice = camera;
            surfaceViewEx.getHolder().setFixedSize(previewSize.getWidth(), previewSize.getHeight());
            //startPreview();
        }

        @Override
        public void onDisconnected(CameraDevice camera) {
            camera.close();
            cameraDevice = null;
        }

        @Override
        public void onError(CameraDevice camera, int error) {
            camera.close();
            cameraDevice = null;
        }
    };
    @SuppressLint("NewApi")
    CameraCaptureSession.StateCallback stateCallback = new CameraCaptureSession.StateCallback() {
        @Override
        public void onConfigured(CameraCaptureSession session) {
            captureSession = session;
            // 自动对焦
            previewBuild.set(CaptureRequest.CONTROL_AF_MODE, CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_PICTURE);
            // 打开闪光灯
            previewBuild.set(CaptureRequest.CONTROL_AE_MODE, CaptureRequest.CONTROL_AE_MODE_ON_AUTO_FLASH);
            captureRequest = previewBuild.build();
            try {
                captureSession.setRepeatingRequest(captureRequest, null, handler);
            } catch (CameraAccessException e) {
                e.printStackTrace();
            }
        }

        @Override
        public void onConfigureFailed(CameraCaptureSession session) {
            captureSession = session;
            captureSession.close();
            captureSession = null;
            cameraDevice.close();
            cameraDevice = null;
        }
    };

    @SuppressLint("NewApi")
    CameraCaptureSession.CaptureCallback captureCallback = new CameraCaptureSession.CaptureCallback() {
        @Override
        public void onCaptureCompleted(CameraCaptureSession session, CaptureRequest request, TotalCaptureResult result) {
            super.onCaptureCompleted(session, request, result);

        }

        @Override
        public void onCaptureProgressed(CameraCaptureSession session, CaptureRequest request, CaptureResult partialResult) {
            super.onCaptureProgressed(session, request, partialResult);

        }

        @Override
        public void onCaptureFailed(CameraCaptureSession session, CaptureRequest request, CaptureFailure failure) {
            super.onCaptureFailed(session, request, failure);

        }
    };
//    ImageReader imageReader;


    private Camera camera;
    private class StreamTask extends AsyncTask<Void, Void, Void>{

        private byte[] data;

        public StreamTask(byte[] data) {
            this.data = data;
        }

        @Override
        protected Void doInBackground(Void... params) {
            if(data != null){
                NDKImpl.encodeYUV(data);
            }
            Log.e("asytask", this.toString());
            return null;
        }
    }
    private StreamTask streamTask;
    private Camera.PreviewCallback previewCallback;
    private boolean isPlay = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        outputUrl = getIntent().getStringExtra("outputUrl");
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_camera);
        unbinder = ButterKnife.bind(this);
        initView();

        previewCallback = new Camera.PreviewCallback() {
            @Override
            public void onPreviewFrame(byte[] data, Camera camera) {
                if(streamTask != null){
                    switch (streamTask.getStatus()){
                        case RUNNING:
                            break;
                        case PENDING:
                            streamTask.cancel(false);
                            break;
                    }
                }
                if(isPlay) {
                    Log.e(CameraActivity.class.getSimpleName(), "" + data.length);
                    NDKImpl.encodeYUV(data);
//                    streamTask = new StreamTask(data);
//                    streamTask.execute((Void) null);
                }
            }
        };

//        camera = Camera.open(0);
    }

    private void initView(){
        surfaceViewEx.getHolder().addCallback(this);
        surfaceViewEx.getHolder().setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);

        final Thread audioThread = new Thread(new AudioRunnable());
        ckb_play.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                isPlay = isChecked;
                if(audioRecord != null) {
                    if (isPlay) {
                        audioRecord.startRecording();
                        audioThread.start();
                    }
                    else{
                        audioRecord.stop();
                    }
                }
                NDKImpl.initStartTime();
            }
        });
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        initCamera(width, height);
        initAudioRecord();
//        initFFMpeg();
//        if(camera != null){
//            try {
//                camera.setPreviewCallback(previewCallback);
//                camera.setPreviewDisplay(holder);
//            } catch (IOException e) {
//                e.printStackTrace();
//            }
//        }
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
            imageReader = ImageReader.newInstance(previewSize.getWidth(), previewSize.getHeight(), ImageFormat.YUV_420_888, 1);
            imageReader.setOnImageAvailableListener(new ImageReader.OnImageAvailableListener() {
                @Override
                public void onImageAvailable(ImageReader reader) {
                    Image image = reader.acquireNextImage();

                    if(isPlay) {
//                        encodePCM();
//                        NDKImpl.encodeYUV(YUVUtils.getDataFromImage(image, YUVUtils.COLOR_FormatNV21));
//                        Log.e(CameraActivity.class.getSimpleName(), "yuv starttime: " + System.currentTimeMillis());
                        ByteBuffer[] yuvBuffer = new ByteBuffer[image.getPlanes().length];
                        byte[][] yuvbytes = new byte[image.getPlanes().length][];
                        for(int i = 0; i < image.getPlanes().length; i++){
                            yuvBuffer[i] = image.getPlanes()[i].getBuffer();
                            yuvbytes[i] = new byte[yuvBuffer[i].remaining()];
                            yuvBuffer[i].get(yuvbytes[i]);
                        }
                        NDKImpl.encodeYUV1(yuvbytes[0], yuvbytes[1], yuvbytes[2], image.getPlanes()[1].getRowStride(), image.getPlanes()[1].getPixelStride());
//                        Log.e(CameraActivity.class.getSimpleName(), "yuv endtime: " + System.currentTimeMillis());
//                        streamTask = new StreamTask(bytes);
//                        streamTask.execute((Void) null);
                    }

                    image.close();

                }
            }, handler);
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
            HandlerThread thread = new HandlerThread("camera2");
            thread.start();
            handler = new Handler(thread.getLooper());
            cameraManager.openCamera(cameraIds[0], cameraCallback, null);
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
//        for(Size size : sizes){
//            if(size.getWidth() * size.getHeight() > maxSize.getWidth() * maxSize.getHeight()) {
//                Log.e(CameraActivity.class.getSimpleName(), "w: " + size.getWidth() + " h: " + size.getHeight());
//                maxSize = size;
//            }
//        }
        return maxSize = new Size(1280, 720);
    }

    private void initAudioRecord(){
        audioBufSize = AudioRecord.getMinBufferSize(sampleRate, channelConfig, audioFormat);
        if(audioBufSize == AudioRecord.ERROR_BAD_VALUE){
            Log.e(CameraActivity.class.getSimpleName(), "audio param is wrong");
            return;
        }
        audioRecord = new AudioRecord(audioSource, sampleRate, channelConfig, audioFormat, audioBufSize);
        if(audioRecord.getState() == AudioRecord.STATE_UNINITIALIZED){
            Log.e(CameraActivity.class.getSimpleName(), "init audio failed");
            return;
        }
    }



    @Override
    protected void onResume() {
        super.onResume();
    }

    @Override
    @SuppressLint("NewApi")
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.e(this.getClass().getSimpleName(), "on surfacechanged");
        if(width == previewSize.getWidth() && height == previewSize.getHeight()){
            initFFMpeg();
            startPreview();
        }

//        if(camera != null){
//            Camera.Parameters parameters = camera.getParameters();
//            parameters.setPreviewSize(CameraActivity.this.width, CameraActivity.this.height);
//            parameters.setPictureSize(CameraActivity.this.width, CameraActivity.this.height);
//            camera.setParameters(parameters);
//            camera.startPreview();
//        }
    }

    @SuppressLint("NewApi")
    private void startPreview(){
        try {
            previewBuild = cameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
            previewBuild.addTarget(surfaceViewEx.getHolder().getSurface());
            previewBuild.addTarget(imageReader.getSurface());
            cameraDevice.createCaptureSession(Arrays.asList(surfaceViewEx.getHolder().getSurface(), imageReader.getSurface()), stateCallback, handler);
        } catch (CameraAccessException e) {
            e.printStackTrace();
        }
    }

    @SuppressLint("NewApi")
    private void initFFMpeg() {
        DisplayMetrics dm = getResources().getDisplayMetrics();
//        width = dm.widthPixels;
//        height = dm.heightPixels;
        width = 320;
        height = 240;
        NDKImpl.initFFMpeg(outputUrl, previewSize.getWidth(), previewSize.getHeight()/*640, 480*/);
//        NDKImpl.initFFMpeg(outputUrl, width, height/*640, 480*/);
    }


    @Override
    protected void onPause() {
        super.onPause();
       // NDKImpl.flush();
        NDKImpl.close();
    }

    @SuppressLint("NewApi")
    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        if(cameraDevice != null) {
            cameraDevice.close();
            cameraDevice = null;
        }
        if(camera != null){
            camera.stopPreview();
            camera.release();
            camera = null;
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        unbinder.unbind();
    }

    private class AudioRunnable implements Runnable{

        @Override
        public void run() {
            Log.e(CameraActivity.class.getSimpleName(), "isPlay: " + isPlay);
            while(isPlay){
                //byte[] audioData = new byte[audioBufSize];
                encodePCM();

            }
        }
    }

    private void encodePCM() {
        FloatBuffer audioBuf = FloatBuffer.allocate(audioBufSize);
        if (audioRecord.getRecordingState() == AudioRecord.RECORDSTATE_RECORDING) {
//            Log.e(CameraActivity.class.getSimpleName(), "starttime: " + System.currentTimeMillis());
//            byte[] sample = getAudioData(audioBuf);
            float[] sample = getAudioData(audioBuf);
            if (sample != null && (sample[2] != 0 && sample[3] != 0)) {
                NDKImpl.encodePCM(sample, sample.length);
//                Log.e(CameraActivity.class.getSimpleName(), "endtime: " + System.currentTimeMillis());
            }
        }
    }

    @SuppressLint("NewApi")
    private float[] getAudioData(FloatBuffer audioData){
        float[] sample = null;
        int ret = audioRecord.read(audioData.array(), 0, audioData.capacity(), READ_BLOCKING);
        if(ret ==  AudioRecord.ERROR_INVALID_OPERATION || ret == AudioRecord.ERROR_BAD_VALUE){
            Log.e(CameraActivity.class.getSimpleName(), "get audio failed");
            return sample;
        }
        audioData.limit(ret);
        return audioData.array();
//        float[] temp = audioData.array();
//        sample = new byte[ret * 4];
//        for(int index = 0; index < ret; index++){
//            float2byte(sample, temp[index], index);
//        }
//        return sample;
    }

    private void float2byte(byte[] b, float s, int i){
        int fInt = Float.floatToIntBits(s);
        for(int j = 0; j < 4; j++){
            b[i * 4 + j] = (byte)((fInt >> (j * 8)) & 0xff);
        }
    }
}
