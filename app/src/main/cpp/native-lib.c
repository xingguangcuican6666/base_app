#include <jni.h>
#include <string.h>
#include <stdio.h>
#include <pwd.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <android/log.h>

#define LOG_TAG "Baseline_Audit"
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

#define __NR_raw_faccessat 48
#define AT_FDCWD -100

// 依然保留 Raw Syscall 作为对比
long raw_access(const char *path) {
#if defined(__aarch64__)
    long ret;
    __asm__ __volatile__ (
        "mov x0, %1\n"
        "mov x1, %2\n"
        "mov x2, %3\n"
        "mov x3, #0\n"
        "mov x8, %4\n"
        "svc #0\n"
        "mov %0, x0\n"
        : "=r"(ret)
        : "r"((long)AT_FDCWD), "r"(path), "r"((long)F_OK), "r"((long)__NR_raw_faccessat)
        : "x0", "x1", "x2", "x3", "x8", "memory"
    );
    return ret;
#else
    return -999;
#endif
}

/**
 * 遍历 Android 应用 UID 空间 (10000 - 19999)
 */
void audit_uid_space() {
    LOGW("=== STARTING UID SPACE SCAN ===");
    int found_count = 0;

    for (int uid = 10000; uid < 20000; uid++) {
        struct passwd *pw = getpwuid(uid);
        if (pw != NULL) {
            found_count++;
            
            // 记录下每一个能搜到的 UID 和它的 Home 目录
            // 在纯净系统上，你应该能看到一大堆 app_... 
            
            if (strstr(pw->pw_name, "termux") || strstr(pw->pw_dir, "com.termux")) {
                LOGW("[MATCH] Found Potential Target: Name=%s, UID=%d, Dir=%s", 
                     pw->pw_name, uid, pw->pw_dir);
                
                // 检查这个存在的 UID，它的目录在内核眼里如何
                long res = raw_access(pw->pw_dir);
                LOGW("      -> Kernel Path Access Result: %ld", res);
            }
        }
    }
    LOGW("=== SCAN FINISHED (Found %d valid UIDs) ===", found_count);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    audit_uid_space();
    return JNI_VERSION_1_6;
}

JNIEXPORT jstring JNICALL
Java_com_example_baseapp_MainActivity_stringFromJNI(JNIEnv *env, jobject thiz) {
    return (*env)->NewStringUTF(env, "Baseline Audit Running...");
}
