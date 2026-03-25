#include <jni.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <errno.h>
#include <sys/syscall.h>
#include <android/log.h>

#define LOG_TAG "Logic_Audit_Core"
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

#define __NR_raw_faccessat 48
#define AT_FDCWD -100

/**
 * ARM64 原始系统调用
 */
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
 * 逻辑审计：寻找 UID 存在但路径消失的矛盾
 */
void perform_logic_conflict_audit(const char* pkg_name) {
    LOGW("--- Testing Logic Consistency: %s ---", pkg_name);

    // 1. 从系统用户数据库 (passwd) 获取信息
    // 这是系统层面对应用身份的最终承认
    struct passwd *pw = getpwnam(pkg_name);
    
    if (pw == NULL) {
        LOGW("[Step 1] User Database: No record for %s (App not installed).", pkg_name);
        return;
    }

    LOGW("[Step 1] User Database: FOUND! UID=%d, Home=%s", pw->pw_uid, pw->pw_dir);

    // 2. 使用 Raw Syscall 探测该 Home 目录
    long syscall_res = raw_access(pw->pw_dir);

    // 3. 交叉分析
    if (syscall_res == 0) {
        LOGW("[Step 2] Kernel View: Path is visible. (No Isolation)");
    } else if (syscall_res == -2) { // ENOENT
        LOGW("[Step 2] Kernel View: Path is HIDDEN (ENOENT).");
        LOGW("[RESULT] LOGIC CONFLICT! System says it exists at %s, but Kernel says it's GONE.", pw->pw_dir);
        LOGW("[Target] This is the effect of Mount Namespace or HMA.");
    } else if (syscall_res == -13) { // EACCES
        LOGW("[Step 2] Kernel View: Path is RESTRICTED (EACCES). (Standard Permission Issue)");
    } else {
        LOGW("[Step 2] Kernel View: Error %ld", syscall_res);
    }
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGW("=== STARTING CROSS-LOGIC AUDIT ===");

    // 探测 Termux
    perform_logic_conflict_audit("com.termux");
    
    // 探测一个肯定不存在的应用作为对照
    perform_logic_conflict_audit("com.this.is.fake.app");

    LOGW("=== CROSS-LOGIC AUDIT FINISHED ===");
    return JNI_VERSION_1_6;
}

JNIEXPORT jstring JNICALL
Java_com_example_baseapp_MainActivity_stringFromJNI(JNIEnv *env, jobject thiz) {
    return (*env)->NewStringUTF(env, "Logic Audit Running...");
}
