# XML位置和生成文件名称以及输出路径
set(SERVICE_INTERFACE_XML "${CMAKE_SOURCE_DIR}/Common/Interfaces/com.example.Training.xml")
set(SERVICE_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(SERVICE_GENERATED_BASENAME "${SERVICE_GENERATED_DIR}/training-generated")
set(SERVICE_GENERATED_C "${SERVICE_GENERATED_BASENAME}.c")
set(SERVICE_GENERATED_H "${SERVICE_GENERATED_BASENAME}.h")
set(CLIENT_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/client-generated")
set(CLIENT_GENERATED_BASENAME "${CLIENT_GENERATED_DIR}/training-generated")
set(CLIENT_GENERATED_C "${CLIENT_GENERATED_BASENAME}.c")
set(CLIENT_GENERATED_H "${CLIENT_GENERATED_BASENAME}.h")

# 调用工具生成
add_custom_command(
    OUTPUT
        "${SERVICE_GENERATED_C}"
        "${SERVICE_GENERATED_H}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${SERVICE_GENERATED_DIR}"
    COMMAND
        "${GDBUS_CODEGEN_EXECUTABLE}"
        --interface-prefix com.example.
        --generate-c-code "${SERVICE_GENERATED_BASENAME}"
        "${SERVICE_INTERFACE_XML}"
    DEPENDS
        "${SERVICE_INTERFACE_XML}"
    COMMENT "Generating GDBus bindings for ServiceProject"
)

add_custom_command(
    OUTPUT
        "${CLIENT_GENERATED_C}"
        "${CLIENT_GENERATED_H}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CLIENT_GENERATED_DIR}"
    COMMAND
        "${GDBUS_CODEGEN_EXECUTABLE}"
        --interface-prefix com.example.
        --generate-c-code "${CLIENT_GENERATED_BASENAME}"
        "${SERVICE_INTERFACE_XML}"
    DEPENDS
        "${SERVICE_INTERFACE_XML}"
    COMMENT "Generating client-side GDBus bindings for ServiceProject libtraining"
)

# 将生成的文件打包为静态库
add_library(service_gdbus_generated STATIC "${SERVICE_GENERATED_C}")
set_source_files_properties("${SERVICE_GENERATED_C}" PROPERTIES LANGUAGE C) # 明确源文件为C文件

target_include_directories(service_gdbus_generated
    PUBLIC
        "${SERVICE_GENERATED_DIR}"
)

target_link_libraries(service_gdbus_generated
    PUBLIC
        PkgConfig::GLIB
)

add_library(client_gdbus_generated STATIC "${CLIENT_GENERATED_C}")
set_source_files_properties("${CLIENT_GENERATED_C}" PROPERTIES LANGUAGE C)

target_include_directories(client_gdbus_generated
    PUBLIC
        "${CLIENT_GENERATED_DIR}"
)

target_link_libraries(client_gdbus_generated
    PUBLIC
        PkgConfig::GLIB
)

set_target_properties(client_gdbus_generated PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

add_library(training_server_core STATIC
    Sources/TrainingState.cpp
    Sources/TrainingService.cpp
)

set_target_properties(training_server_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(training_server_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)

target_link_libraries(training_server_core
    PUBLIC
        training_common
        training_utils
        service_gdbus_generated
)

add_library(training SHARED
    Sources/training/TrainingLibrary.cpp
)

target_include_directories(training
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)

target_link_libraries(training
    PUBLIC
        training_common
        training_utils
        client_gdbus_generated
)

set_target_properties(training PROPERTIES
    OUTPUT_NAME training
)

# 打包为可执行文件
add_subdirectory(app)
