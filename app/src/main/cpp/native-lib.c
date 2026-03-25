#include <jni.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <pwd.h>
#include <unistd.h>
#include <android/log.h>

#define LOG_TAG "HMA_Audit_Core"
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define TARGET_PKG "com.termux"

// 执行深度行为审计
void run_behavioral_audit() {
    LOGW("=== Starting HMA Behavioral Inconsistency Audit ===");

    struct stat st;
    char path[128];
    snprintf(path, sizeof(path), "/data/data/%s", TARGET_PKG);

    // 方法 A: 路径矛盾探测 (The Errno Side-channel)
    // 正常系统：不存在报 ENOENT(2)；HMA隐藏：有时报 EACCES(13)
    int res = stat(path, &st);
    if (res == 0) {
        LOGW("[Evidence A] Path is VISIBLE. UID: %d", st.st_uid);
    } else {
        if (errno == EACCES) {
            LOGW("[Evidence B] HMA CONFIRMED: Path returns EACCES (Permission Denied). It exists but is being masked!");
        } else {
            LOGW("[Status] Path stat returned errno: %d (%s)", errno, strerror(errno));
        }
    }

    // 方法 B: 符号链接差异 (Symlink Bypass)
    // HMA 可能只 Hook 了 /data/data，漏掉了 /data/user/0
    char alt_path[128];
    snprintf(alt_path, sizeof(alt_path), "/data/user/0/%s", TARGET_PKG);
    if (access(alt_path, F_OK) == 0 && res != 0) {
        LOGW("[Evidence C] HMA INCONSISTENCY: Accessible via /data/user/0 but hidden via /data/data!");
    }

    // 方法 C: 内核 UID 孤儿探测 (Kernel Orphaned UID)
    // 遍历 UID。如果内核有流量记录(/proc/uid_stat)但包名被抹除，说明 HMA 正在工作
    for (int uid = 10000; uid < 11000; uid++) {
        char proc_path[64];
        snprintf(proc_path, sizeof(proc_path), "/proc/uid_stat/%d", uid);
        if (access(proc_path, F_OK) == 0) {
            struct passwd *pw = getpwuid(uid);
            if (pw == NULL) {
                LOGW("[Evidence D] HMA DETECTED: Orphaned UID %d found in Kernel, but Package Name is erased from User DB.", uid);
            }
        }
    }

    // 方法 D: 内存指纹探测 (LSPosed/HMA Maps)
    FILE* maps = fopen("/proc/self/maps", "r");
    if (maps) {
        char line[512];
        while (fgets(line, sizeof(line), maps)) {
            if (strstr(line, "lsposed") || strstr(line, "libcom.hma") || strstr(line, "riru")) {
                LOGW("[Evidence E] Hook Library in Memory: %s", line);
            }
        }
        fclose(maps);
    }

    LOGW("=== Audit Finished ===");
}

// SO 加载时自动触发
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    run_behavioral_audit();
    return JNI_VERSION_1_6;
}

// 修复后的 stringFromJNI
JNIEXPORT jstring JNICALL
Java_com_example_baseapp_MainActivity_stringFromJNI(JNIEnv *env, jobject thiz) {
    // C 语言环境下必须传递 env 
    return (*env)->NewStringUTF(env, "Native Audit executed. Check 'HMA_Audit_Core' in Logcat.");
}
