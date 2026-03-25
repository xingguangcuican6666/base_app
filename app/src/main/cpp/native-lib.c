#include <jni.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>
#include <dirent.h>
#include <android/log.h>

#define LOG_TAG "HMA_Final_Audit"
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

/**
 * 模糊路径探测：利用 errno 差异实锤 HMA 目录
 */
void audit_hma_random_dir() {
    LOGW("=== Starting HMA Random Dir Probing ===");

    // 虽然我们不能 ls /data/misc，但如果我们猜中了前缀，系统会返回 13 而不是 2
    // 仓库特征：hide_my_applist_ + 随机字符串
    const char* base_path = "/data/misc/hide_my_applist_";
    
    // 方案：尝试探测几个可能的变体，或者直接探测父目录的权限反应
    struct stat st;
    if (stat("/data/misc/hide_my_applist_config", &st) == -1) {
        if (errno == EACCES) {
            LOGW("[!!] HMA DETECTED: /data/misc/hide_my_applist_config exists (Access Denied)");
        }
    }

    // 重点：探测你发现的那个具体目录（测试用）
    if (stat("/data/misc/hide_my_applist_mlbhplcsdqpghyib", &st) == -1) {
        if (errno == EACCES) {
            LOGW("[!!!] HMA CONFIRMED: Random suffix dir found via EACCES trap!");
        } else if (errno == ENOENT) {
            LOGW("[-] Path not found (ENOENT).");
        }
    }
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    // 运行模糊探测
    audit_hma_random_dir();
    
    // 补充：检查当前进程是否被注入了 HMA 的 Java 代理
    // 即使目录名随机，内存里的包名字符串通常是固定的
    FILE* fp = fopen("/proc/self/maps", "r");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, "hma_oss")) {
                LOGW("[!!!] HMA MEMORY FOUND: %s", line);
            }
        }
        fclose(fp);
    }

    return JNI_VERSION_1_6;
}

JNIEXPORT jstring JNICALL
Java_com_example_baseapp_MainActivity_stringFromJNI(JNIEnv *env, jobject thiz) {
    return (*env)->NewStringUTF(env, "HMA Random Dir Audit Complete.");
}
