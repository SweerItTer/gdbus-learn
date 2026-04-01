# XML位置和生成文件名称以及输出路径
set(SERVICE_INTERFACE_XML "${CMAKE_SOURCE_DIR}/Common/Interfaces/com.example.Training.xml")
set(SERVICE_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(SERVICE_GENERATED_BASENAME "${SERVICE_GENERATED_DIR}/training-generated")
set(SERVICE_GENERATED_C "${SERVICE_GENERATED_BASENAME}.c")
set(SERVICE_GENERATED_H "${SERVICE_GENERATED_BASENAME}.h")

# 调用工具自动生成文件
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

# 将生成的文件打包为静态库
add_library(gdbus_generated STATIC "${SERVICE_GENERATED_C}")
set_source_files_properties("${SERVICE_GENERATED_C}" PROPERTIES LANGUAGE C) # 明确源文件为C文件

# 导入修改头文件
target_include_directories(gdbus_generated
    PUBLIC
        "${SERVICE_GENERATED_DIR}"
)

# 链接第三方库
target_link_libraries(gdbus_generated
    PUBLIC
        PkgConfig::GLIB
)

# 核心静态库
add_library(core STATIC
    Sources/TrainingService.cpp
)

# 独立于位置
set_target_properties(core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# 添加头文件
target_include_directories(core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)

# 链接其他库
target_link_libraries(core
    PUBLIC
        training_common
        training_utils
        gdbus_generated
)

# 动态库 
add_subdirectory(Library)
# 可执行文件
add_subdirectory(app)
