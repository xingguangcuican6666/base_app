#include <jni.h>
#include <android/log.h>
#include <string.h>

#define LOG_TAG "BaseApp-Native"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* JNI_OnLoad is called when the .so is loaded via System.loadLibrary() */
JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGI("JNI_OnLoad called - native-lib.so is being loaded");
    return JNI_VERSION_1_6;
}

/* JNI_OnUnload is called when the .so is unloaded */
JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) {
    LOGI("JNI_OnUnload called - native-lib.so is being unloaded");
}

/*
 * Class:     com_example_baseapp_MainActivity
 * Method:    stringFromJNI
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_example_baseapp_MainActivity_stringFromJNI(JNIEnv *env, jobject thiz) {
    const char *message = "Hello from native C! SO loaded successfully.";
    LOGI("stringFromJNI called - returning: %s", message);
    return (*env)->NewStringUTF(env, message);
}
