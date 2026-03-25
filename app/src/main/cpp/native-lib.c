#include <jni.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <unistd.h>
#include <android/log.h>

#define LOG_TAG "HMA_Audit_Core"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

// 探测目标
#define TARGET_PKG "com.termux"

/**
 * 核心审计：寻找系统逻辑自相矛盾点
 */
void run_behavioral_audit() {
    LOGI("--- Starting Integrated Behavioral Audit ---");

    struct stat st;
    char path[128];
    snprintf(path, sizeof(path), "/data/data/%s", TARGET_PKG);

    // 1. 路径矛盾探测 (The "Ghost" Path)
    // 即使 HMA 隐藏了目录，底层的 stat 系统调用有时会返回不同的错误码
    int res = stat(path, &st);
    if (res == 0) {
        LOGW("[Result] Evidence A: Path is VISIBLE (HMA not active or bypassed). UID: %d", st.st_uid);
    } else {
        if (errno == EACCES) {
            // 关键：如果报错是“权限拒绝”而不是“文件不存在”，说明路径拦截不彻底
            LOGW("[Result] Evidence B: HMA Detected! Path returns EACCES (Permission Denied). System knows it's there but hides it.");
        } else if (errno == ENOENT) {
            LOGI("[Result] Info: Path returns ENOENT. HMA or System Sandbox is effective.");
        }
    }

    // 2. 符号链接绕过审计 (Symlink Inconsistency)
    // 尝试访问不同的链接路径。HMA 有时只 Hook 了 /data/data
    const char* alt_path = "/data/user/0/" TARGET_PKG;
    if (access(alt_path, F_OK) == 0 && res != 0) {
        LOGW("[Result] Evidence C: HMA Inconsistency! Found via /data/user/0 but hidden in /data/data.");
    }

    // 3. 内核 UID 残留审计 (Kernel Level Leak)
    // 遍历猜测 Termux 可能的 UID (通常在 10000-20000 之间)
    // 即使包名被隐藏，内核流量统计目录 /proc/uid_stat/<UID> 通常不会被 Hook
    for (int uid = 10000; uid < 11000; uid++) {
        char proc_uid_path[64];
        snprintf(proc_uid_path, sizeof(proc_uid_path), "/proc/uid_stat/%d", uid);
        if (access(proc_uid_path, F_OK) == 0) {
            // 发现了一个活跃 UID，尝试反查包名
            struct passwd *pw = getpwuid(uid);
            if (pw == NULL) {
                // 如果内核有 UID 记录，但 getpwuid 查不到包名，说明包名被 HMA 强行抹除了
                LOGW("[Result] Evidence D: HMA Confirmed! Orphaned UID %d found in kernel but missing in user database.", uid);
            }
        }
    }

    // 4. 内存映射审计 (LSPosed/HMA Library Check)
    FILE* maps = fopen("/proc/self/maps", "r");
    if (maps) {
        char line[512];
        while (fgets(line, sizeof(line), maps)) {
            if (strstr(line, "lsposed") || strstr(line, "libcom.hma") || strstr(line, "riru")) {
                LOGW("[Result] Evidence E: Hook Framework found in memory: %s", line);
            }
        }
        fclose(maps);
    }

    LOGI("--- Audit Finished ---");
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    // 自动触发，无需等待 Java 调用
    run_behavioral_audit();
    return JNI_VERSION_1_6;
}

JNIEXPORT jstring JNICALL
Java_com_example_baseapp_MainActivity_stringFromJNI(JNIEnv *env, jobject thiz) {
    return (*env)->NewStringUTF("Audit logic executed in JNI_OnLoad. Check Logcat.");
}
