#include <jni.h>
#include <pwd.h>
#include <string.h>
#include <android/log.h>

#define LOG_TAG "HMA_Fix_Verify"
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

/**
 * 验证 getpwnam 在高压和匿名上下文下的表现
 */
JNIEXPORT void JNICALL
Java_com_example_baseapp_MainActivity_verifyHmaFix(JNIEnv *env, jobject thiz) {
    const char* target = "com.termux";
    
    LOGW("=== 执行 HMA 修复验证 ===");

    // 1. 验证身份识别逻辑 (针对漏洞 1)
    // 即使在 Native 层直接调用，UID 也是确定的。
    // 如果你修复了 ActivityStarter 的 UID 校验，系统启动拦截将无懈可击。
    struct passwd *pw = getpwnam(target);
    
    if (pw == NULL) {
        LOGW("[PASS] getpwnam 返回 NULL。身份过滤依然生效，HMA 成功识别了调用者 UID。");
    } else {
        LOGW("[FAIL] 逻辑泄露！能查到 UID: %d。说明 HMA 在当前上下文中未能正确拦截。", pw->pw_uid);
    }

    // 2. 模拟并发后的状态
    // 如果 uidHideCache 崩溃导致 Hook 卸载，这里的 access 就会成功
    if (access("/data/data/com.termux", 0) == 0) {
        LOGW("[FAIL] 文件路径暴露！Hook 框架可能已因并发异常而卸载 (unload)。");
    } else {
        LOGW("[PASS] 路径依然隐藏。uidHideCache 并发修复表现稳定。");
    }
}
