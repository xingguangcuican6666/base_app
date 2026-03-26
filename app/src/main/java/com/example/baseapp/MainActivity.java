package com.example.baseapp;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("native-lib");
    }

    // JNI 接口：冷热启动差异分析，判定目标包是否被本地 Hook 隐藏
    public native void testColdHot();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // 软件启动时执行冷热差异分析
        testColdHot();
    }
}
