#include <jni.h>
#include <time.h>
#include <pthread.h>
#include <stdbool.h>
#include <android/log.h>

#define LOG_TAG "HMA_Stress"
#define THREAD_COUNT 4  // 干扰线程数
#define ITERATIONS 50

static JavaVM *g_vm = NULL;
static jobject g_pm_obj = NULL;
static jmethodID g_mid = NULL;
static bool keep_running = false;

// 获取纳秒
static long get_now_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000L + ts.tv_nsec;
}

// 干扰线程：疯狂轰炸系统 PMS 服务
void* noise_worker(void* arg) {
    JNIEnv *env;
    if ((*g_vm)->AttachCurrentThread(g_vm, &env, NULL) != JNI_OK) return NULL;

    jstring fake_pkg = (*env)->NewStringUTF(env, "com.pms.stress.test.random");

    while (keep_running) {
        // 持续查询不存在的包，强迫系统 PMS 进行全量搜索并产生 Binder 等待
        (*env)->CallObjectMethod(env, g_pm_obj, g_mid, fake_pkg, 0);
        if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    }

    (*env)->DeleteLocalRef(env, fake_pkg);
    (*g_vm)->DetachCurrentThread(g_vm);
    return NULL;
}

// 核心测量逻辑
long perform_measurement(JNIEnv *env, const char* pkg_name) {
    jstring j_pkg = (*env)->NewStringUTF(env, pkg_name);
    long start = get_now_ns();

    (*env)->CallObjectMethod(env, g_pm_obj, g_mid, j_pkg, 0);
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);

    long duration = get_now_ns() - start;
    (*env)->DeleteLocalRef(env, j_pkg);
    return duration;
}

JNIEXPORT void JNICALL
Java_com_example_baseapp_MainActivity_runStressTest(JNIEnv *env, jobject thiz, jstring target_pkg_j) {
    const char* target_pkg = (*env)->GetStringUTFChars(env, target_pkg_j, NULL);
    const char* fake_pkg = "com.non.existent.random.path";

    // 1. 获取全局引用 (初始化)
    (*env)->GetJavaVM(env, &g_vm);
    jclass context_cls = (*env)->GetObjectClass(env, thiz);
    jmethodID get_pm_mid = (*env)->GetMethodID(env, context_cls, "getPackageManager", "()Landroid/content/pm/PackageManager;");
    jobject pm_local = (*env)->CallObjectMethod(env, thiz, get_pm_mid);
    if (!pm_local) {
        (*env)->ReleaseStringUTFChars(env, target_pkg_j, target_pkg);
        return;
    }
    g_pm_obj = (*env)->NewGlobalRef(env, pm_local);
    jclass pm_cls = (*env)->GetObjectClass(env, g_pm_obj);
    g_mid = (*env)->GetMethodID(env, pm_cls, "getPackageInfo", "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;");

    // --- 第一步：空闲状态测量 (Idle) ---
    long idle_fake = 0, idle_target = 0;
    for (int i = 0; i < ITERATIONS; i++) idle_fake += perform_measurement(env, fake_pkg);
    for (int i = 0; i < ITERATIONS; i++) idle_target += perform_measurement(env, target_pkg);
    idle_fake /= ITERATIONS;
    idle_target /= ITERATIONS;

    // --- 第二步：高负载状态测量 (Stress) ---
    keep_running = true;
    pthread_t threads[THREAD_COUNT];
    int thread_started[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; i++) {
        thread_started[i] = (pthread_create(&threads[i], NULL, noise_worker, NULL) == 0);
    }

    // 给干扰线程一点时间占领 Binder 线程池
    struct timespec sleep_ts = {0, 100 * 1000000}; // 100ms
    nanosleep(&sleep_ts, NULL);

    long stress_fake = 0, stress_target = 0;
    for (int i = 0; i < ITERATIONS; i++) stress_fake += perform_measurement(env, fake_pkg);
    for (int i = 0; i < ITERATIONS; i++) stress_target += perform_measurement(env, target_pkg);
    stress_fake /= ITERATIONS;
    stress_target /= ITERATIONS;

    // 停止干扰
    keep_running = false;
    for (int i = 0; i < THREAD_COUNT; i++) {
        if (thread_started[i]) pthread_join(threads[i], NULL);
    }

    // --- 第三步：输出判定报告 ---
    // Guard against division by zero (near-instant measurements)
    if (idle_fake == 0) idle_fake = 1;
    if (idle_target == 0) idle_target = 1;
    float ratio_fake = (float)stress_fake / idle_fake;
    float ratio_target = (float)stress_target / idle_target;

    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "==== 并发竞争报告 ====");
    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "Fake 包延迟增长率: %.2fx", ratio_fake);
    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "Target 包延迟增长率: %.2fx", ratio_target);

    if (ratio_target < 1.5f && ratio_fake > 3.0f) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "判定：【实锤隐藏】。目标包无视系统 Binder 压力，表现出本地 Hook 特征。");
    } else {
        __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "判定：两者同步增长，符合真实不存在的特征。");
    }

    (*env)->ReleaseStringUTFChars(env, target_pkg_j, target_pkg);
    (*env)->DeleteGlobalRef(env, g_pm_obj);
    g_pm_obj = NULL;
}
