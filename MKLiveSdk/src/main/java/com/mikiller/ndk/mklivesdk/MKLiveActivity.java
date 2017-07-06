package com.mikiller.ndk.mklivesdk;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.support.v7.app.AlertDialog;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.ImageButton;

/**
 * Created by Mikiller on 2016/12/2.
 */
@SuppressLint("NewApi")
public class MKLiveActivity extends AppCompatActivity {
    private static final String TAG = MKLiveActivity.class.getSimpleName();

    SurfaceView surfaceViewEx;
    ImageButton switchCamerabutton;
    ImageButton flashCamerabutton;
    CheckBox ckb_play;

    boolean isNetWorkError = false;
    private MKLiveSDK liveSDK;
    boolean flashOn = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        liveSDK = MKLiveSDK.getInstance(this);
        liveSDK.setOrientation(this);
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_camera);
        surfaceViewEx = (SurfaceView) findViewById(R.id.surfaceViewEx);
        switchCamerabutton = (ImageButton) findViewById(R.id.SwitchCamerabutton);
        flashCamerabutton = (ImageButton) findViewById(R.id.FlashCamerabutton);
        ckb_play = (CheckBox) findViewById(R.id.ckb_play);

        liveSDK.init(new MKLiveSDK.EncodeCallback() {
            @Override
            public void onPermissionFailed(String permission) {
                new AlertDialog.Builder(MKLiveActivity.this).setTitle("需要权限").setMessage(permission + "权限被禁止").setPositiveButton("确定", null).show();
            }

            @Override
            public void onEncodeFailed(final int error) {

                ckb_play.post(new Runnable() {
                    @Override
                    public void run() {
                        if (ckb_play != null)
                            ckb_play.setChecked(false);
                        if (error == MKLiveSDK.NETWORK_ERR) {
                            isNetWorkError = true;
                            new AlertDialog.Builder(MKLiveActivity.this).setTitle("网络故障").setMessage("直播中断，请检查网络").setPositiveButton("确定", null).show();
                        }
                    }
                });

            }
        }, surfaceViewEx.getHolder());

        initView(liveSDK.getPreViewWidth(), liveSDK.getPreViewHeight());
    }

    private void initView(final int width, final int height) {
        surfaceViewEx.getHolder().addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(SurfaceHolder holder) {
                Log.e(TAG, "surface created");
                holder.setFixedSize(width, height);
                holder.setKeepScreenOn(true);
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
                if (width == w) {
                    liveSDK.openCamera();
                }
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder) {
                Log.e(TAG, "surface destoryed");
                holder.getSurface().release();
            }
        });

        ckb_play.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(CompoundButton buttonView, boolean isChecked) {
                if (!isChecked) {
                    Log.e(TAG, "pause video thread");
                    liveSDK.pause();
                    buttonView.setText(" 开 始 ");
                } else {
                    if (isNetWorkError) {
                        liveSDK.initFFMpeg();
                        isNetWorkError = false;
                    }
                    Log.e(TAG, "start video thread");
                    liveSDK.start();
                    buttonView.setText(" 暂 停 ");

                }
            }
        });

        switchCamerabutton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                liveSDK.switchCamera();
            }
        });

        flashCamerabutton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                liveSDK.switchFlash(flashOn = !flashOn);
            }
        });

    }

    @Override
    protected void onResume() {
        super.onResume();
    }

    @Override
    protected void onPause() {
        super.onPause();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        liveSDK.onDestory();

    }


}
