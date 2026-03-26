#include <jni.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <android/log.h>

#define LOG_TAG "HMA_Probe"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// 获取高精度纳秒时间
static long get_now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000L + ts.tv_nsec;
}

// 核心：单次 API 查询计时
long measure_once(JNIEnv *env, jobject pm_obj, jmethodID mid, jstring j_pkg) {
    long start = get_now_ns();

    // 调用 getPackageInfo(pkg, 0)
    // 注意：在 Android 11+ 上，如果没在 Manifest 声明 queries，
    // 查询不存在或隐藏的包都会抛出 NameNotFoundException。
    (*env)->CallObjectMethod(env, pm_obj, mid, j_pkg, 0);

    // 捕获并清除异常（这是计时的终点）
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
    }

    return get_now_ns() - start;
}

JNIEXPORT jstring JNICALL
Java_com_example_baseapp_MainActivity_nativeInit(JNIEnv *env, jobject thiz) {
    // 1. 反射获取 PackageManager
    jclass context_cls = (*env)->GetObjectClass(env, thiz);
    jmethodID get_pm_mid = (*env)->GetMethodID(env, context_cls, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    (*env)->DeleteLocalRef(env, context_cls);
    if (!get_pm_mid) return (*env)->NewStringUTF(env, "错误: 无法获取 getPackageManager 方法");

    jobject pm_obj = (*env)->CallObjectMethod(env, thiz, get_pm_mid);
    if (!pm_obj) return (*env)->NewStringUTF(env, "错误: 无法获取 PackageManager 实例");

    jclass pm_cls = (*env)->GetObjectClass(env, pm_obj);
    jmethodID get_info_mid = (*env)->GetMethodID(env, pm_cls, "getPackageInfo", "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;");
    (*env)->DeleteLocalRef(env, pm_cls);
    if (!get_info_mid) {
        (*env)->DeleteLocalRef(env, pm_obj);
        return (*env)->NewStringUTF(env, "错误: 无法获取 getPackageInfo 方法");
    }

    // 2. 准备探测目标
    const char* pkgs[] = {
        "com.android.settings",            // 基准：系统应用（存在）
        "com.random.fake.path.test",       // 基准：真不存在
        "com.topjohnwu.magisk"             // 目标：隐藏应用
    };

    char final_report[1024] = "探测报告:\n";

    for (int p = 0; p < 3; p++) {
        jstring j_pkg = (*env)->NewStringUTF(env, pkgs[p]);
        long total_ns = 0;
        int samples = 100;

        // --- 热身阶段 ---
        // 消除 JIT 编译和 HMA 初次加载缓存的抖动
        for (int i = 0; i < 10; i++) measure_once(env, pm_obj, get_info_mid, j_pkg);

        // --- 采样阶段 ---
        for (int i = 0; i < samples; i++) {
            total_ns += measure_once(env, pm_obj, get_info_mid, j_pkg);
        }

        long avg = total_ns / samples;
        LOGD("包名: %s | 平均耗时: %ld ns", pkgs[p], avg);

        char line[128];
        snprintf(line, sizeof(line), "%s: %ld ns\n", pkgs[p], avg);
        strncat(final_report, line, sizeof(final_report) - strlen(final_report) - 1);

        (*env)->DeleteLocalRef(env, j_pkg);
    }

    (*env)->DeleteLocalRef(env, pm_obj);
    return (*env)->NewStringUTF(env, final_report);
}
