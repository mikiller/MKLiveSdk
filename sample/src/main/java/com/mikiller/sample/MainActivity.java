package com.mikiller.sample;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RadioButton;
import android.widget.RadioGroup;

import com.mikiller.ndk.mklivesdk.MKLiveSDK;

import java.io.File;
import java.io.IOException;

import butterknife.BindView;
import butterknife.ButterKnife;
import butterknife.Unbinder;

public class MainActivity extends AppCompatActivity {

    private Unbinder unbinder;
    @BindView(R.id.rdg_video_quality)
    RadioGroup rdg_video_quality;
    @BindView(R.id.rdg_audio_quality)
    RadioGroup rdg_audio_quality;
    @BindView(R.id.rdb_sd)
    public RadioButton rdb_sd;
    @BindView(R.id.rdb_hd)
    public RadioButton rdb_hd;
    @BindView(R.id.rdb_op)
    public RadioButton rdb_op;
    @BindView(R.id.rdb_fl)
    public RadioButton rdb_fl;
    @BindView(R.id.rdb_st)
    public RadioButton rdb_st;
    @BindView(R.id.rdb_hq)
    public RadioButton rdb_hq;
    @BindView(R.id.rdg_screenOrientation)
    public RadioGroup rdg_screenOrientation;
    @BindView(R.id.rdg_camera)
    public RadioGroup rdg_camera;
    @BindView(R.id.edt_input)
    public EditText edt_input;
    @BindView(R.id.edt_output)
    public EditText edt_output;
    @BindView(R.id.btn_start_camera)
    public Button btn_start_camera;

    private String fileDir;
    private String filePath;
    MKLiveSDK liveSdk;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        unbinder = ButterKnife.bind(this);

        fileDir = getExternalFilesDir(null).getAbsolutePath().concat(File.separator);
        filePath = fileDir.concat(edt_input.getText().toString());

        edt_input.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {

            }

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {

            }

            @Override
            public void afterTextChanged(Editable s) {
                if(!TextUtils.isEmpty(s.toString())){
                    switch (s.toString()){
                        case "1.flv":
                        case "1.mp4":
                        case "1.aac":
                            filePath = fileDir.concat(s.toString());
                            File file = new File(filePath);
                            if(!file.exists()){
                                try {
                                    file.createNewFile();
                                } catch (IOException e) {
                                    e.printStackTrace();
                                }
                            }
                            break;
                    }

                }
            }
        });

//        btn_start.setOnClickListener(new View.OnClickListener() {
//            @Override
//            public void onClick(View v) {
//                if(TextUtils.isEmpty(edt_input.getText().toString()) || TextUtils.isEmpty(edt_output.getText().toString()))
//                    return;
//                //filePath = filePath.concat(edt_input.getText().toString());
//                File file = new File(filePath);
//                if(!file.exists()){
//                    Toast.makeText(MainActivity.this, "file not exit\n" + filePath, Toast.LENGTH_LONG).show();
//                    return;
//                }
//                outputUrl = edt_output.getText().toString();
//
//                NDKImpl.pushRTMP(filePath, outputUrl);
//            }
//        });

        liveSdk = MKLiveSDK.getInstance();
        btn_start_camera.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                int videoQuality = Integer.parseInt((String) findViewById(rdg_video_quality.getCheckedRadioButtonId()).getTag());
                int audioQuality = Integer.parseInt((String) findViewById(rdg_audio_quality.getCheckedRadioButtonId()).getTag());
                int orientation = Integer.parseInt((String)findViewById(rdg_screenOrientation.getCheckedRadioButtonId()).getTag());
                int cameraId = Integer.parseInt((String)findViewById(rdg_camera.getCheckedRadioButtonId()).getTag());

                liveSdk.setLiveArgs(edt_output.getText().toString(), videoQuality, cameraId, orientation, audioQuality);
                try {
                    liveSdk.callLiveActivity(MainActivity.this, null);
                } catch (IllegalAccessException e) {
                    e.printStackTrace();
                }
            }
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        unbinder.unbind();
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    //public native String stringFromJNI();

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("ndktest");
//        System.loadLibrary("FFMpegSDK");
//        System.loadLibrary("ndk2");
    }
}
