#include <jni.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <android/log.h>

#define LOG_TAG "RootProbe"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// 核心探测函数：区分“权限拒绝”和“路径不存在”
int probe_path(const char* path) {
    if (faccessat(AT_FDCWD, path, F_OK, AT_SYMLINK_NOFOLLOW) == 0) {
        return 0; // 居然能访问（通常由于权限位 0700，普通应用会失败）
    }
    return errno; // 返回具体的错误码：EACCES (13) 或 ENOENT (2)
}

JNIEXPORT void JNICALL
Java_com_example_baseapp_MainActivity_nativeInit(JNIEnv *env, jobject thiz) {
    
    // 1. 基准测试：系统自带的目录
    int err_adb = probe_path("/data/adb");
    
    // 2. 目标测试：Magisk/KernelSU 特有的子目录
    int err_modules = probe_path("/data/adb/modules");
    int err_ksu = probe_path("/data/adb/ksu");

    LOGD("--- 环境分析开始 ---");
    LOGD("基准路径 [/data/adb]: errno = %d (%s)", err_adb, strerror(err_adb));
    LOGD("探测路径 [/data/adb/modules]: errno = %d (%s)", err_modules, strerror(err_modules));
    LOGD("探测路径 [/data/adb/ksu]: errno = %d (%s)", err_ksu, strerror(err_ksu));

    // 3. 逻辑判定
    if (err_adb == EACCES) {
        if (err_modules == EACCES || err_ksu == EACCES) {
            LOGD("判定结果: [不洁] - 发现隐藏的子目录索引。");
        } else if (err_modules == ENOENT) {
            LOGD("判定结果: [干净] - /data/adb 为系统原生空目录。");
        }
    } else if (err_adb == ENOENT) {
        LOGD("判定结果: [干净] - 连 /data/adb 都不存在。");
    } else {
        LOGD("判定结果: [未知] - 受到更高级别的挂载或 SELinux 屏蔽。");
    }
    LOGD("--- 环境分析结束 ---");
}
