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

// 兼容性修复：如果 SYS_fstatat 没定义，尝试使用 __NR_fstatat64 (ARM64 常用)
#if !defined(SYS_fstatat)
  #if defined(__NR_fstatat64)
    #define SYS_fstatat __NR_fstatat64
  #elif defined(__aarch64__)
    #define SYS_fstatat 79  // ARM64 架构下 fstatat 的原始系统调用号
  #endif
#endif

// 直接读取 ARM64 虚拟计数器
static inline uint64_t read_ticks() {
    uint64_t val;
#if defined(__aarch64__)
    asm volatile("mrs %0, cntvct_el0" : "=r" (val));
#else
    val = 0; 
#endif
    return val;
}

// 使用裸系统调用执行 fstatat
static inline int raw_stat(const char *path) {
    struct stat st;
    // 使用 (long) 转换以适配 syscall 参数要求
    return syscall((long)SYS_fstatat, (long)AT_FDCWD, (long)path, (long)&st, (long)0);
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
        raw_stat(normal_path); 
        s = read_ticks();
        raw_stat(normal_path);
        e = read_ticks();
        normal_total += (e - s);

        // 测量敏感路径
        raw_stat(ksu_path); 
        s = read_ticks();
        raw_stat(ksu_path);
        e = read_ticks();
        ksu_total += (e - s);
    }

    uint64_t avg_normal = normal_total / ITERATIONS;
    uint64_t avg_ksu = ksu_total / ITERATIONS;
    double ratio = (double)avg_ksu / (double)avg_normal;

    // 修复警告：使用 %lu 打印 uint64_t (在 ARM64 对应 unsigned long)
    // 或者强制转换为 unsigned long long 使用 %llu
    __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "结果统计: Normal=%llu, KSU=%llu, Ratio=%.2f", 
                        (unsigned long long)avg_normal, 
                        (unsigned long long)avg_ksu, 
                        ratio);

    if (ratio > 1.35) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, 
            "!!! DETECTED !!! 路径响应耗时异常偏高 (Ratio: %.2f)", ratio);
    } else {
        __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "系统调用响应均匀。");
    }
}
