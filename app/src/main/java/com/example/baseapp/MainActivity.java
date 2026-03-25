package com.example.baseapp;

import androidx.appcompat.app.AppCompatActivity;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = "HMA_Stability_Test";

    // 原有的 JNI 方法
    public native String stringFromJNI();
    
    // 新增：用于验证 HMA 修复效果的 Native 审计方法
    // 逻辑：在 C 层通过 getpwnam 和 access 探测逻辑矛盾
    public native void verifyHmaFix();

    static {
        System.loadLibrary("native-lib");
        Log.i(TAG, "核心审计库 native-lib.so 加载成功");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        TextView tv = findViewById(R.id.tv_result);
        tv.setText("正在进行 HMA 稳定性与逻辑审计...\n请查看 Logcat (TAG: " + TAG + ")");

        // 1. 立即执行一次静态审计
        Log.w(TAG, ">>> 执行初始静态审计 <<<");
        verifyHmaFix();

        // 2. 启动多线程饱和攻击测试 (针对 uidHideCache 并发漏洞)
        startConcurrencyStressTest();
    }

    /**
     * 饱和攻击测试：
     * 模拟 16 个线程并发访问 PMS，试图触发 uidHideCache 的 ConcurrentModificationException。
     * 如果修复成功，HMA 不会崩溃，且 verifyHmaFix() 始终返回 NULL。
     */
    private void startConcurrencyStressTest() {
        final String targetPkg = "com.termux"; // 假设这是被隐藏的目标
        int threadCount = 16;
        ExecutorService executor = Executors.newFixedThreadPool(threadCount);

        Log.w(TAG, ">>> 启动并发压力测试：16 线程同时冲击 PMS 钩子 <<<");

        for (int i = 0; i < threadCount; i++) {
            executor.execute(() -> {
                int iterations = 0;
                while (iterations < 500) { // 每个线程跑 500 次
                    try {
                        // 触发 HMA 内部的 uidHideCache 读取和写入
                        getPackageManager().getPackageInfo(targetPkg, 0);
                        
                        // 间歇性调用 Native 审计，看 Hook 是否因为并发异常而中途卸载
                        if (iterations % 100 == 0) {
                            verifyHmaFix();
                        }
                    } catch (PackageManager.NameNotFoundException e) {
                        // 预期结果：HMA 正常工作时，应该报找不到包
                    } catch (Exception e) {
                        Log.e(TAG, "！！！检测到异常中断，可能是 uidHideCache 并发修复失效！！！", e);
                    }
                    iterations++;
                }
                Log.i(TAG, Thread.currentThread().getName() + " 并发序列执行完毕");
            });
        }
    }
}
