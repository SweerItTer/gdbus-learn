cmake_minimum_required(VERSION 3.16)
project(dbus_exercise LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)

# 寻找依赖
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET gio-2.0 gio-unix-2.0 glib-2.0 gobject-2.0)

find_program(GDBUS_CODEGEN_EXECUTABLE gdbus-codegen REQUIRED)

enable_testing()

set(TRAINING_SERVER_INSTALL_DIR "${CMAKE_INSTALL_FULL_LIBEXECDIR}/training")
set(TRAINING_SYSTEMD_UNIT_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/deploy/training.service")
set(TRAINING_DBUS_POLICY_INSTALL_DIR "/usr/share/dbus-1/system.d" CACHE PATH
    "Install directory for the D-Bus system bus policy file")
set(TRAINING_SYSTEMD_UNIT_INSTALL_DIR "/usr/lib/systemd/system" CACHE PATH
    "Install directory for the systemd unit")

configure_file(
    "${CMAKE_SOURCE_DIR}/deploy/systemd/training.service.in"
    "${TRAINING_SYSTEMD_UNIT_OUTPUT}"
    @ONLY
)

install(
    FILES "${CMAKE_SOURCE_DIR}/deploy/dbus/com.example.Training.conf"
    DESTINATION "${TRAINING_DBUS_POLICY_INSTALL_DIR}"
)

install(
    FILES "${TRAINING_SYSTEMD_UNIT_OUTPUT}"
    DESTINATION "${TRAINING_SYSTEMD_UNIT_INSTALL_DIR}"
)

# 添加子模块
add_subdirectory(Common)            # 通用模块 
add_subdirectory(ServiceProject)    # 服务端
add_subdirectory(ClientProject)     # 客户端
