#include <jni.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <android/log.h>

#define LOG_TAG "HMA_Nuclear_Audit"
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

/**
 * 1. 物理路径审计 (Config & Logs)
 * 仓库显示配置存放在 /data/misc/hide_my_applist_*
 * 虽然普通 App 没权限 ls /data/misc，但我们可以尝试 stat 具体文件
 */
void audit_hma_files() {
    const char* hma_targets[] = {
        "/data/misc/hide_my_applist_config",
        "/data/misc/hide_my_applist_prefs",
        "/data/misc/hide_my_applist_records",
        "/data/misc/hide_my_applist_log"
    };

    for (int i = 0; i < 4; i++) {
        struct stat st;
        if (stat(hma_targets[i], &st) == 0) {
            LOGW("[Evidence-File] HMA Artifact Found: %s", hma_targets[i]);
        } else {
            // 如果返回 EACCES (13)，说明路径存在但被 SELinux 拦截，这也是证据
            if (errno == EACCES) {
                LOGW("[Evidence-File] HMA Trace (EACCES): %s exists but restricted.", hma_targets[i]);
            }
        }
    }
}

/**
 * 2. 内存特征审计 (Library & Framework)
 * 检查当前进程映射，寻找 Xposed 或 HMA 注入的 .so 或 .dex
 */
void audit_hma_memory() {
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) return;

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        // 针对仓库中的包名特征进行过滤
        if (strstr(line, "hma_oss") || strstr(line, "lsposed") || strstr(line, "xposed")) {
            LOGW("[Evidence-Mem] HMA/Xposed Framework Mapping: %s", line);
        }
    }
    fclose(fp);
}

/**
 * 3. Content Provider 侧信道探测
 * 仓库提到：org.frknkrc44.hma_oss.ServiceProvider
 * 我们在 C 层通过 shell 命令进行盲测，看该 Provider 是否响应
 */
void audit_hma_provider() {
    char buf[128];
    // 使用 content query 探测，如果不报错或返回特定信息，则 HMA 存在
    FILE* fp = popen("content query --uri content://org.frknkrc44.hma_oss.ServiceProvider --short_format 2>/dev/null", "r");
    if (fp) {
        if (fgets(buf, sizeof(buf), fp) != NULL) {
            LOGW("[Evidence-Provider] HMA ServiceProvider is ACTIVE.");
        }
        pclose(fp);
    }
}

/**
 * 4. UID 交叉审计 (Package Presence)
 * 仓库提到包名：org.frknkrc44.hma_oss
 * 即使它隐藏了别人，它自己如果没被“自我隐藏”，UID 反查就能抓到它
 */
void audit_hma_uid() {
    for (int i = 10000; i < 11000; i++) {
        // getpwnam 在 C 层有时能绕过 Java 层的包名过滤
        struct passwd *pw = getpwuid(i);
        if (pw && strstr(pw->pw_name, "hma_oss")) {
            LOGW("[Evidence-UID] HMA Package UID Detected: %d (%s)", i, pw->pw_name);
        }
    }
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGW("=== HMA-OSS REVERSE AUDIT START ===");

    audit_hma_files();      // 审计物理残留
    audit_hma_memory();     // 审计内存注入
    audit_hma_provider();   // 审计组件特征
    audit_hma_uid();        // 审计 UID 映射

    LOGW("=== HMA-OSS REVERSE AUDIT FINISHED ===");
    return JNI_VERSION_1_6;
}

JNIEXPORT jstring JNICALL
Java_com_example_baseapp_MainActivity_stringFromJNI(JNIEnv *env, jobject thiz) {
    return (*env)->NewStringUTF(env, "HMA-OSS Specialized Audit Complete.");
}
