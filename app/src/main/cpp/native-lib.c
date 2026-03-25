#include <jni.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <android/log.h>

#define LOG_TAG "HMA_Syscall_Audit"
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

/**
 * 使用 ARM64 内联汇编直接触发 faccessat 系统调用
 * syscall number: 48 (faccessat)
 * 参数: dirfd, pathname, mode, flags
 */
long raw_faccessat(int dirfd, const char *pathname, int mode, int flags) {
    long ret;
    register int r0 __asm__("w0") = dirfd;
    register const char *r1 __asm__("x1") = pathname;
    register int r2 __asm__("w2") = mode;
    register int r3 __asm__("w3") = flags;
    register int r8 __asm__("x8") = 48; // faccessat 的系统调用号

    __asm__ __volatile__ (
        "svc #0"
        : "=r"(r0)
        : "r"(r0), "r"(r1), "r"(r2), "r"(r3), "r"(r8)
        : "memory"
    );
    ret = r0;
    return ret;
}

void perform_syscall_contrast_audit(const char* target_path) {
    // 1. 标准 libc 调用 (极可能被 HMA Hook)
    int libc_res = access(target_path, F_OK);
    int libc_err = errno;

    // 2. 原始系统调用 (绕过 libc Hook)
    // AT_FDCWD 表示相对当前工作目录，或者使用绝对路径
    long syscall_res = raw_faccessat(-100, target_path, F_OK, 0);

    LOGW("--- Contrast Audit for: %s ---", target_path);
    LOGW("[Libc Result]: %d (errno: %d)", libc_res, libc_err);
    LOGW("[Syscall Result]: %ld", syscall_res);

    // 逻辑矛盾判定
    if (libc_res != 0 && syscall_res == 0) {
        LOGW("[!!!] HMA DETECTED: Libc was lied to, but Kernel knows the truth!");
    } else if (libc_res == 0 && syscall_res == 0) {
        LOGW("[Status] App is visible to both (HMA not active for this path).");
    } else {
        LOGW("[Status] Both agree it's gone (Target likely not installed).");
    }
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    // 探测 Termux (即使它被隐藏)
    perform_syscall_contrast_audit("/data/data/com.termux");
    return JNI_VERSION_1_6;
}
