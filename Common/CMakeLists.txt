add_library(training_common INTERFACE)

target_include_directories(training_common
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)

add_library(training_utils STATIC
    Sources/utils/ScopedBusConnection.cpp
    Sources/utils/ScopedBusNameOwner.cpp
    Sources/utils/ScopedExportedInterface.cpp
)

set_target_properties(training_utils PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

target_include_directories(training_utils
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)

target_link_libraries(training_utils
    PUBLIC
        PkgConfig::GLIB
)
