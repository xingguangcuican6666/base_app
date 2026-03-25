#include <jni.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <android/log.h>

#define LOG_TAG "BaseApp_Native"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define TARGET_PKG "com.termux"

// --- 审计工具函数 ---

// 尝试绕过目录遍历限制，直接探测路径状态
void perform_path_audit(const char* pkg) {
    struct stat st;
    char path[128];
    snprintf(path, sizeof(path), "/data/data/%s", pkg);
    
    if (stat(path, &st) == 0) {
        LOGI("[Audit] SUCCESS: Path %s exists. UID assigned: %d", path, st.st_uid);
        // 如果能拿到 UID，说明该包在系统分区中真实存在，未被彻底卸载
    } else {
        // ENOENT (2) 表示没有该文件，EACCES (13) 表示权限被拒绝（但路径可能存在）
        LOGI("[Audit] INFO: Path %s stat failed. Error: %s (%d)", path, strerror(errno), errno);
    }
}

// 侧信道：尝试调用底层 pm 命令（有时 Hook 无法覆盖到 runtime exec）
void perform_cmd_audit(const char* pkg) {
    char cmd[128];
    char buf[256];
    snprintf(cmd, sizeof(cmd), "pm path %s 2>&1", pkg);
    FILE* fp = popen(cmd, "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp) != NULL) {
            LOGI("[Audit] PM_CMD Output: %s", buf);
        } else {
            LOGI("[Audit] PM_CMD: No output (Package hidden or missing)");
        }
        pclose(fp);
    }
}

// --- JNI 生命周期与方法 ---

// 1. SO 加载时自动调用
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("------------------------------------------");
    LOGI("JNI_OnLoad: SO library is loading...");
    
    // 执行自动审计
    LOGI("[Auto-Audit] Target: %s", TARGET_PKG);
    perform_path_audit(TARGET_PKG);
    perform_cmd_audit(TARGET_PKG);
    
    LOGI("JNI_OnLoad: Audit finished. Returning JNI_VERSION_1_6");
    LOGI("------------------------------------------");
    return JNI_VERSION_1_6;
}

// 2. SO 卸载时调用
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
    LOGI("JNI_OnUnload: SO library is being unloaded.");
}

// 3. 原有的 stringFromJNI 实现
JNIEXPORT jstring JNICALL
Java_com_example_baseapp_MainActivity_stringFromJNI(JNIEnv *env, jobject thiz) {
    LOGI("Java_com_example_baseapp_MainActivity_stringFromJNI called.");
    
    // 构造一个简单的返回结果，包含审计目标的当前状态
    char response[128];
    struct stat st;
    if (stat("/data/data/" TARGET_PKG, &st) == 0) {
        snprintf(response, sizeof(response), "Audit: %s FOUND (UID %d)", TARGET_PKG, st.st_uid);
    } else {
        snprintf(response, sizeof(response), "Audit: %s NOT_FOUND", TARGET_PKG);
    }
    
    return (*env)->NewStringUTF(env, response);
}
