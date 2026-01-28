cmake_minimum_required(VERSION 3.28)
project(project_qemu)

set(CMAKE_C_STANDARD 23)

include_directories(include)

add_executable(project_qemu src/main.c
        src/Loader.c
        src/Library.c
        src/CryptoUtil.c
        src/DynamicLinker.c
        src/List.c
        src/QemuDefines.c)
