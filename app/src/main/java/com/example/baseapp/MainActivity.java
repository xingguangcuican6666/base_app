package com.example.baseapp;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = "HMA_Probe";

    static {
        System.loadLibrary("native-lib");
    }

    // JNI 接口：返回探测报告字符串
    public native String nativeInit();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // 软件启动时执行探测并记录结果
        String report = nativeInit();
        Log.d(TAG, report);
    }
}
