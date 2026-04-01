cmake_minimum_required(VERSION 3.16)
project(dbus_exercise LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 寻找依赖
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET gio-2.0 gio-unix-2.0 glib-2.0 gobject-2.0)

find_program(GDBUS_CODEGEN_EXECUTABLE gdbus-codegen REQUIRED)

enable_testing()

# 添加子模块
add_subdirectory(Common)            # 通用模块 
add_subdirectory(ServiceProject)    # 服务端
add_subdirectory(ClientProject)     # 客户端
