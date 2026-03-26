#include <jni.h>
#include <sys/stat.h> // 必须包含这个头文件
#include <fcntl.h>    // 包含 AT_FDCWD
#include <errno.h>
#include <string.h>
#include <android/log.h>

#define LOG_TAG "RootProbe"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// 使用 fstatat 代替 faccessat，兼容性更好
int probe_path_v2(const char* path) {
    struct stat st;
    // AT_FDCWD 表示相对于当前工作目录（由于是绝对路径，会直接解析）
    // AT_SYMLINK_NOFOLLOW 防止符号链接误导
    if (fstatat(AT_FDCWD, path, &st, AT_SYMLINK_NOFOLLOW) == 0) {
        return 0; // 存在且可读属性（通常应用层会报 13）
    }
    return errno; // 我们要的就是这个 errno
}

JNIEXPORT void JNICALL
Java_com_example_baseapp_MainActivity_nativeInit(JNIEnv *env, jobject thiz) {
    
    int err_adb = probe_path_v2("/data/adb");
    int err_modules = probe_path_v2("/data/adb/modules");
    int err_ksu = probe_path_v2("/data/adb/ksu");

    LOGD("--- 环境分析开始 (V2) ---");
    LOGD("基准路径 [/data/adb]: errno = %d (%s)", err_adb, strerror(err_adb));
    LOGD("探测路径 [/data/adb/modules]: errno = %d (%s)", err_modules, strerror(err_modules));
    LOGD("探测路径 [/data/adb/ksu]: errno = %d (%s)", err_ksu, strerror(err_ksu));

    // 逻辑判定：
    // 即使是 700 权限且 SELinux 拦截，fstatat 在 VFS 查找阶段
    // 如果文件真的不存在，会优先返回 ENOENT (2)
    // 如果文件存在但由于父目录无权限进入，会返回 EACCES (13)
    if (err_adb == 13) {
        if (err_modules == 13 || err_ksu == 13) {
            LOGD("判定结果: [不洁] - 发现隐藏的子目录索引。");
        } else if (err_modules == 2) {
            LOGD("判定结果: [干净] - 路径探测确认内部无 modules。");
        }
    } else {
        LOGD("判定结果: [环境异常] - 可能是由于 Namespace 隔离导致无法触达 /data/adb");
    }
    LOGD("--- 环境分析结束 ---");
}
