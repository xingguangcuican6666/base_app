#include <jni.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/syscall.h>
#include <android/log.h>

#define LOG_TAG "HMA_Nuclear_Audit"
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

// 定义常用的 ARM64 系统调用号 (避免头文件定义冲突)
#define __NR_raw_faccessat 48
#define AT_FDCWD -100

/**
 * 原始系统调用实现 (仅限 ARM64)
 * 使用内联汇编绕过 libc.so 的所有 Hook
 */
long perform_raw_syscall_access(const char *pathname) {
#if defined(__aarch64__)
    long ret;
    // 参数说明: dirfd (AT_FDCWD), pathname, mode (F_OK), flags (0)
    // 寄存器使用: x0-x3 传参, x8 存放调用号, svc #0 触发中断
    __asm__ __volatile__ (
        "mov x0, %1\n"
        "mov x1, %2\n"
        "mov x2, %3\n"
        "mov x3, #0\n"
        "mov x8, %4\n"
        "svc #0\n"
        "mov %0, x0\n"
        : "=r"(ret)
        : "r"((long)AT_FDCWD), "r"(pathname), "r"((long)F_OK), "r"((long)__NR_raw_faccessat)
        : "x0", "x1", "x2", "x3", "x8", "memory"
    );
    return ret;
#else
    return -999; // 非 ARM64 环境返回标识
#endif
}

/**
 * 核心审计函数：对比 Libc 与 Syscall 的结果
 */
void do_contrast_audit(const char* pkg_name) {
    char path[128];
    snprintf(path, sizeof(path), "/data/data/%s", pkg_name);

    // 1. 标准 Libc 调用 (HMA 的主场)
    // 此时 errno 会被更新
    int libc_res = access(path, F_OK);
    int libc_err = errno;

    // 2. Raw Syscall 调用 (内核的真理)
    // 返回值直接就是结果或错误码 (负数表示错误)
    long syscall_res = perform_raw_syscall_access(path);

    LOGW("--------------------------------------------");
    LOGW("[Audit Target]: %s", path);
    LOGW("[Standard Libc]: res=%d, errno=%d (%s)", libc_res, libc_err, strerror(libc_err));
    LOGW("[Raw Syscall  ]: res=%ld", syscall_res);

    // --- 逻辑判断逻辑 ---
    
    // 情况 A: Libc 说不存在 (-1), 但内核 Syscall 说存在 (0)
    if (libc_res != 0 && syscall_res == 0) {
        LOGW("[!!!] EVIDENCE FOUND: HMA is spoofing this process! Libc is hooked.");
    } 
    // 情况 B: 两者都返回错误，但错误码不一致
    // 注意：Syscall 返回的是负的 errno (如 -2 表示 ENOENT)
    else if (libc_res != 0 && syscall_res != (long)libc_res) {
        if (syscall_res == -13) { // EACCES
            LOGW("[!!!] TRAP: Kernel returned EACCES (13), but Libc returned ENOENT (2). HMA detected.");
        }
    }
    // 情况 C: 全部一致
    else {
        LOGW("[Status]: No inconsistency detected for this path.");
    }
}

/**
 * JNI_OnLoad: SO 加载时自动触发审计
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGW("=== STARTING FULL RAW SYSCALL AUDIT ===");

    // 审计目标：Termux 和 HMA 自身
    do_contrast_audit("com.termux");
    do_contrast_audit("org.frknkrc44.hma_oss");

    LOGW("=== AUDIT COMPLETED ===");
    return JNI_VERSION_1_6;
}

/**
 * 修复后的 stringFromJNI
 */
JNIEXPORT jstring JNICALL
Java_com_example_baseapp_MainActivity_stringFromJNI(JNIEnv *env, jobject thiz) {
    return (*env)->NewStringUTF(env, "Check Logcat for 'HMA_Nuclear_Audit:W'");
}
