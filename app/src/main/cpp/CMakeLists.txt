cmake_minimum_required(VERSION 3.22.1)

project("native-lib" C)

# 创建共享库（.so 文件）
add_library(
        native-lib
        SHARED
        native-lib.c
)

# 查找 Android 日志库
find_library(
        log-lib
        log
)

# 链接 Android 日志库
target_link_libraries(
        native-lib
        ${log-lib}
)
