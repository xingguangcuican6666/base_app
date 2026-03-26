package com.example.baseapp;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("native-lib");
    }

    // JNI 接口：执行深度扫描并通过 Logcat 输出报告
    public native void runFullReport();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // 软件启动时执行深度扫描
        runFullReport();
    }
}
