#include <jni.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <pwd.h>
#include <errno.h>
#include <unistd.h>
#include <android/log.h>

#define LOG_TAG "HMA_Audit_Core"
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

/**
 * 核心逻辑：扫描内核网络连接表
 * 寻找那些“活着”但“看不见”的 UID
 */
void audit_kernel_ghost_uids() {
    LOGW("=== Starting Global Kernel UID Audit ===");

    // 读取内核 TCP 状态表，这是全局可见的
    FILE* fp = fopen("/proc/net/tcp", "r");
    if (!fp) {
        LOGW("[-] Cannot open /proc/net/tcp, checking /proc/net/udp");
        fp = fopen("/proc/net/udp", "r");
    }

    if (fp) {
        char line[512];
        int line_count = 0;
        while (fgets(line, sizeof(line), fp)) {
            if (line_count++ == 0) continue; // 跳过标题行

            int sl, local_port, rem_port, state, uid;
            char local_addr[128], rem_addr[128];

            // 格式解析：sl: local_address rem_address st tx_queue rx_queue tr tm-ns retr uid ...
            // 我们只需要第 8 个字段：UID
            if (sscanf(line, "%d: %[^:]:%x %[^:]:%x %x %*s %*s %*s %*s %*s %d", 
                       &sl, local_addr, &local_port, rem_addr, &rem_port, &state, &uid) == 7) {
                
                // 仅审计普通应用 UID (10000+)
                if (uid >= 10000 && uid <= 19999) {
                    struct passwd *pw = getpwuid(uid);
                    
                    // 矛盾点 1：内核有连接，但系统说没这个用户
                    if (pw == NULL) {
                        LOGW("[!] HMA EVIDENCE: Found traffic for Ghost UID %d (No Package Name!)", uid);
                        continue;
                    }

                    // 矛盾点 2：能查到包名，但 stat 报“不存在”
                    char pkg_path[128];
                    struct stat st;
                    snprintf(pkg_path, sizeof(pkg_path), "/data/data/%s", pw->pw_name);
                    
                    if (stat(pkg_path, &st) != 0 && errno == ENOENT) {
                        LOGW("[!!] HMA STEALTH DETECTED: UID %d (%s) is ACTIVE but its storage is HIDDEN!", uid, pw->pw_name);
                    }
                }
            }
        }
        fclose(fp);
    }
    LOGW("=== Global Audit Finished ===");
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    audit_kernel_ghost_uids();
    return JNI_VERSION_1_6;
}

JNIEXPORT jstring JNICALL
Java_com_example_baseapp_MainActivity_stringFromJNI(JNIEnv *env, jobject thiz) {
    // 修正 C 语言 JNI 参数
    return (*env)->NewStringUTF(env, "Check Logcat for HMA_Audit_Core:W");
}
