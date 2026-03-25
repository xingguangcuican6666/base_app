#include <jni.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <android/log.h>

#define LOG_TAG "Baseline_Audit"
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

void audit_proc_pid_uids() {
    LOGW("=== STARTING KERNEL PID SCAN ===");
    DIR* dir = opendir("/proc");
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // 过滤非数字目录 (只看 PID)
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;

        char path[64];
        struct stat st;
        snprintf(path, sizeof(path), "/proc/%s", entry->d_name);

        // 获取目录的 stat 信息
        if (stat(path, &st) == 0) {
            // Android 应用 UID 通常在 10000 以上
            if (st.st_uid >= 10000 && st.st_uid <= 19999) {
                // 尝试读取进程名 (cmdline)
                char cmd_path[64];
                char cmdline[128] = {0};
                snprintf(cmd_path, sizeof(cmd_path), "/proc/%s/cmdline", entry->d_name);
                
                FILE* f = fopen(cmd_path, "r");
                if (f) {
                    fgets(cmdline, sizeof(cmdline), f);
                    fclose(f);
                }

                // 如果能读到 cmdline 或者我们关心的 UID
                // 即使 cmdline 为空（被系统屏蔽），UID 依然是硬指纹
                if (strlen(cmdline) > 0) {
                    LOGW("[Found Proc] PID: %s, UID: %d, Cmd: %s", entry->d_name, st.st_uid, cmdline);
                }
            }
        }
    }
    closedir(dir);
    LOGW("=== KERNEL PID SCAN FINISHED ===");
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    audit_proc_pid_uids();
    return JNI_VERSION_1_6;
}
