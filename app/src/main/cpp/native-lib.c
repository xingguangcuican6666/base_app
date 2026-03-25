#include <jni.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <errno.h>
#include <unistd.h>
#include <android/log.h>

#define LOG_TAG "HMA_Audit_Core"
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define TARGET_PKG "com.termux"

// 1. 端口盲刺：绕过文件沙盒，探测 Termux 常见的监听端口 (SSH: 8022)
void audit_network_ports() {
    int ports[] = {8022, 8080}; // Termux 常用端口
    for (int i = 0; i < 2; i++) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(ports[i]);
        addr.sin_addr.s_addr = inet_addr("127.0.0.1");

        // 设置极短超时，避免阻塞
        struct timeval tv = {0, 50000}; // 50ms
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            LOGW("[Evidence-Net] Port %d is OPEN. App exists and service is running.", ports[i]);
        }
        close(sock);
    }
}

// 2. UID 数据库反查：利用系统用户清单绕过目录屏蔽
void audit_uid_database() {
    // 遍历 Android 应用的标准 UID 范围
    for (int i = 10000; i < 11000; i++) {
        struct passwd *pw = getpwuid(i);
        if (pw != NULL && pw->pw_dir != NULL) {
            if (strstr(pw->pw_dir, TARGET_PKG)) {
                LOGW("[Evidence-UID] Found Target in UserDB! UID: %d, Home: %s", i, pw->pw_dir);
            }
        }
    }
}

// 3. 内存映射扫描：寻找 Hook 框架特征（LSPosed/HMA）
void audit_mem_maps() {
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) return;
    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "lsposed") || strstr(line, "libcom.hma") || strstr(line, "riru")) {
            LOGW("[Evidence-Mem] Hook Framework detected in memory: %s", line);
        }
    }
    fclose(fp);
}

// 4. 原有的路径矛盾探测
void audit_path_behavior() {
    struct stat st;
    char path[128];
    snprintf(path, sizeof(path), "/data/data/%s", TARGET_PKG);
    
    if (stat(path, &st) != 0) {
        // 如果是纯净环境且 SDK 28+，这里通常报 errno 2 (ENOENT)
        LOGW("[Status] Path %s is HIDDEN (errno: %d)", path, errno);
    } else {
        LOGW("[Evidence-Path] Path %s is VISIBLE! UID: %d", path, st.st_uid);
    }
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGW("=== COMBINED AUDIT START ===");
    
    audit_path_behavior();  // 维度 A: 文件系统
    audit_uid_database();   // 维度 B: 系统用户库
    audit_network_ports();  // 维度 C: 网络侧信道
    audit_mem_maps();       // 维度 D: 内存指纹

    LOGW("=== COMBINED AUDIT FINISHED ===");
    return JNI_VERSION_1_6;
}

JNIEXPORT jstring JNICALL
Java_com_example_baseapp_MainActivity_stringFromJNI(JNIEnv *env, jobject thiz) {
    // 修正 C 语言中的参数传递
    return (*env)->NewStringUTF(env, "Check Logcat for 'HMA_Audit_Core:W'");
}
