#include <jni.h>
#include <string.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <android/log.h>

struct linux_dirent64 {
    unsigned long long d_ino;
    long long          d_off;
    unsigned short     d_reclen;
    unsigned char      d_type;
    char               d_name[];
};

JNIEXPORT void JNICALL
Java_com_example_baseapp_MainActivity_rawDirectoryScan(JNIEnv *env, jobject thiz) {
    // 直接打开 /data/data 目录
    int fd = open("/data/data", O_RDONLY | O_DIRECTORY);
    if (fd < 0) return;

    char buf[8192];
    int nread;

    // 使用 getdents64 系统调用 (编号 61) 绕过所有 libc 层的 readdir Hook
    while ((nread = syscall(SYS_getdents64, fd, buf, sizeof(buf))) > 0) {
        for (int bpos = 0; bpos < nread; ) {
            struct linux_dirent64 *d = (struct linux_dirent64 *) (buf + bpos);
            if (strstr(d->d_name, "termux")) {
                __android_log_print(ANDROID_LOG_ERROR, "HMA_TERMINATOR", 
                    "!!! NATIVE BREAKTHROUGH !!! 系统调用发现目标: %s", d->d_name);
            }
            bpos += d->d_reclen;
        }
    }
    close(fd);
}
