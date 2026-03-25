#include <jni.h>
#include <pwd.h>
#include <unistd.h>
#include <android/log.h>

#define LOG_TAG "HMA_Stability_Test"
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

JNIEXPORT void JNICALL
Java_com_example_baseapp_MainActivity_verifyHmaFix(JNIEnv *env, jobject thiz) {
    const char* target_pkg = "com.termux";

    // 1. 验证 getpwnam (测试 HMA 对 Identity 的拦截)
    struct passwd *pw = getpwnam(target_pkg);
    if (pw == NULL) {
        // 正常情况：HMA 拦截了查询
    } else {
        LOGW("[CRITICAL] 逻辑漏洞：成功获取了 UID %d，HMA 拦截可能已被绕过！", pw->pw_uid);
    }

    // 2. 验证路径 (测试 PMS 钩子是否因并发异常而失效)
    if (access("/data/data/com.termux", F_OK) == 0) {
        LOGW("[CRITICAL] 路径暴露：发现 Termux 目录，PMS 钩子可能已卸载 (unload)！");
    }
}

// 保留原有的 stringFromJNI 方便 UI 显示
JNIEXPORT jstring JNICALL
Java_com_example_baseapp_MainActivity_stringFromJNI(JNIEnv *env, jobject thiz) {
    return (*env)->NewStringUTF(env, "审计进行中，请观察 Logcat 输出");
}
