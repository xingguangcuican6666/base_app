#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <android/log.h>

void audit_abstract_sockets() {
    LOGW("=== STARTING FIXED ABSTRACT SOCKET SCAN ===");
    
    // 这里的名字不要带 @，代码会手动处理首字节
    const char* targets[] = {
        "termux.auth",
        "termux-tasker",
        "com.termux.am.socket"
    };

    for (int i = 0; i < 3; i++) {
        int sock = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) continue;

        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;

        // 核心修正：
        // 1. sun_path[0] 已经是 \0 (因为 memset)
        // 2. 将名字复制到 sun_path[1] 开始的位置
        size_t name_len = strlen(targets[i]);
        if (name_len > sizeof(addr.sun_path) - 2) name_len = sizeof(addr.sun_path) - 2;
        memcpy(addr.sun_path + 1, targets[i], name_len);

        // 3. 计算准确的地址长度：sun_family 的大小 + 1 (null byte) + 名字长度
        // 不要使用 sizeof(addr)，那是固定长度，会导致内核读取多余的垃圾数据
        socklen_t addr_len = offsetof(struct sockaddr_un, sun_path) + 1 + name_len;

        if (connect(sock, (struct sockaddr*)&addr, addr_len) == 0) {
            LOGW("[MATCH] Found Active Socket: @%s", targets[i]);
        } else {
            // ECONNREFUSED (111) 表示存在监听但拒绝，EACCES (13) 表示被 SELinux 拦截
            if (errno == ECONNREFUSED) {
                LOGW("[MATCH] Detected Occupied Socket: @%s (Refused)", targets[i]);
            } else if (errno == EACCES) {
                LOGW("[INFO] Socket @%s is blocked by SELinux", targets[i]);
            }
        }
        close(sock);
    }
    LOGW("=== SOCKET SCAN FINISHED ===");
}
