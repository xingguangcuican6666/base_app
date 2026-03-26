package com.example.baseapp;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("native-lib");
    }

    // JNI 接口：并发竞争压力测试，判定目标包是否被本地 Hook 隐藏
    public native void runStressTest(String targetPkg);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // 软件启动时对目标包执行压力测试
        runStressTest("com.topjohnwu.magisk");
    }
}
