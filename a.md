~~~
#include <jni.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <android/log.h>

#define LOG_TAG "HMA_DeepScan"
#define ITERATIONS 50

typedef struct {
    long min;
    long max;
    double avg;
    double std_dev; // 标准差：反映波动的剧烈程度
} ScanResult;

// 获取高精度纳秒
static long get_now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000L + ts.tv_nsec;
}

ScanResult run_pkg_scan(JNIEnv *env, jobject pm_obj, jmethodID mid, jstring j_pkg) {
    long times[ITERATIONS];
    double sum = 0;
    long min = 2000000000L;
    long max = 0;

    // 执行 50 次采样
    for (int i = 0; i < ITERATIONS; i++) {
        long start = get_now_ns();
        
        (*env)->CallObjectMethod(env, pm_obj, mid, j_pkg, 0);
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);

        long duration = get_now_ns() - start;
        times[i] = duration;
        sum += duration;
        if (duration < min) min = duration;
        if (duration > max) max = duration;
    }

    double avg = sum / ITERATIONS;
    double variance_sum = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        variance_sum += pow(times[i] - avg, 2);
    }
    
    ScanResult res = {min, max, avg, sqrt(variance_sum / ITERATIONS)};
    return res;
}

JNIEXPORT void JNICALL
Java_com_example_baseapp_MainActivity_runFullReport(JNIEnv *env, jobject thiz) {
    // 1. 获取 PackageManager 环境
    jclass context_cls = (*env)->GetObjectClass(env, thiz);
    jmethodID get_pm_mid = (*env)->GetMethodID(env, context_cls, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    jobject pm_obj = (*env)->CallObjectMethod(env, thiz, get_pm_mid);
    jclass pm_cls = (*env)->GetObjectClass(env, pm_obj);
    jmethodID get_info_mid = (*env)->GetMethodID(env, pm_cls, "getPackageInfo", "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;");

    const char* targets[] = {
        "com.android.settings",      // 基准：存在
        "com.fake.path.random666",   // 基准：不存在
        "com.topjohnwu.magisk"       // 目标：隐藏应用
    };

    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "========== 深度扫描报告 (n=50) ==========");
    
    for(int i=0; i<3; i++) {
        jstring j_pkg = (*env)->NewStringUTF(env, targets[i]);
        ScanResult res = run_pkg_scan(env, pm_obj, get_info_mid, j_pkg);
        
        __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, 
            "目标: %-25s | Min: %5ld | Max: %7ld | Avg: %7.1f | StdDev: %5.1f", 
            targets[i], res.min, res.max, res.avg, res.std_dev);
            
        (*env)->DeleteLocalRef(env, j_pkg);
    }
    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "========================================");
}
~~~
