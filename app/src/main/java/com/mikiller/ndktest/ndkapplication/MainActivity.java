package com.mikiller.ndktest.ndkapplication;

import android.content.Context;
import android.content.Intent;
import android.hardware.Camera;
import android.hardware.camera2.CameraManager;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.text.method.ScrollingMovementMethod;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.TextureView;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import java.io.File;
import java.io.IOException;

import butterknife.BindView;
import butterknife.ButterKnife;
import butterknife.Unbinder;

public class MainActivity extends AppCompatActivity {

    private Unbinder unbinder;
    @BindView(R.id.btn_start)
    public Button btn_start;
    @BindView(R.id.edt_input)
    public EditText edt_input;
    @BindView(R.id.edt_output)
    public EditText edt_output;
    @BindView(R.id.btn_start_camera)
    public Button btn_start_camera;

    private String fileDir;
    private String filePath;
    private String outputUrl;

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

        btn_start.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if(TextUtils.isEmpty(edt_input.getText().toString()) || TextUtils.isEmpty(edt_output.getText().toString()))
                    return;
                //filePath = filePath.concat(edt_input.getText().toString());
                File file = new File(filePath);
                if(!file.exists()){
                    Toast.makeText(MainActivity.this, "file not exit\n" + filePath, Toast.LENGTH_LONG).show();
                    return;
                }
                outputUrl = edt_output.getText().toString();

                NDKImpl.pushRTMP(filePath, outputUrl);
            }
        });

        btn_start_camera.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {

                Intent intent = new Intent(MainActivity.this, CameraActivity.class);
                intent.putExtra("outputUrl", outputUrl = edt_output.getText().toString());
//                intent.putExtra("outputUrl", outputUrl = filePath);
                startActivity(intent);
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
    public native String stringFromJNI();

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("ndktest");
//        System.loadLibrary("ndk2");
    }
}
