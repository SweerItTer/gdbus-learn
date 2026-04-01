# 添加动态库
add_library(training SHARED
    Sources/TrainingLibrary.cpp
)

# 动态库头文件
target_include_directories(training
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)

# 动态库链接
target_link_libraries(training
    PUBLIC
        training_common
        training_utils
        gdbus_generated
)

# 动态库名称
set_target_properties(training PROPERTIES
    OUTPUT_NAME training
)