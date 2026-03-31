#include <jni.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <android/log.h>

#define LOG_TAG "KSU_DETECTOR"
#define ITERATIONS 1000

// 直接读取 ARM64 虚拟计数器，绕过 libc 层的计时函数
static inline uint64_t read_ticks() {
    uint64_t val;
    // 使用 asm 避免编译器优化掉读取指令
    asm volatile("mrs %0, cntvct_el0" : "=r" (val));
    return val;
}

// 使用裸系统调用执行 fstatat (ARM64 编号 79)
// 这样可以绕过 libc.so 中可能被 Hook 的 stat/fstat 函数
static inline int raw_stat(const char *path) {
    struct stat st;
    // fstatat(AT_FDCWD, path, &st, 0)
    return syscall(SYS_fstatat, AT_FDCWD, path, &st, 0);
}

JNIEXPORT void JNICALL
Java_com_example_baseapp_MainActivity_checkKsuTiming(JNIEnv *env, jobject thiz) {
    const char *normal_path = "/system/bin/sh";
    const char *ksu_path = "/system/bin/su";
    
    uint64_t normal_total = 0;
    uint64_t ksu_total = 0;

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "--- 开启高精度时序分析 ---");

    for (int i = 0; i < ITERATIONS; i++) {
        uint64_t s, e;

        // 测量正常路径
        raw_stat(normal_path); // 预热
        s = read_ticks();
        raw_stat(normal_path);
        e = read_ticks();
        normal_total += (e - s);

        // 测量敏感路径
        raw_stat(ksu_path); // 预热
        s = read_ticks();
        raw_stat(ksu_path);
        e = read_ticks();
        ksu_total += (e - s);
    }

    uint64_t avg_normal = normal_total / ITERATIONS;
    uint64_t avg_ksu = ksu_total / ITERATIONS;
    double ratio = (double)avg_ksu / (double)avg_normal;

    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "结果统计: Normal=%llu, KSU=%llu, Ratio=%.2f", 
                        avg_normal, avg_ksu, ratio);

    if (ratio > 1.35) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, 
            "!!! DETECTED !!! 路径响应耗时异常偏高 (Ratio: %.2f)，推测存在内核级 Hook.", ratio);
    } else {
        __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "系统调用响应均匀，未见异常。");
    }
}
