add_library(training_client_wrapper STATIC
    Sources/client/TrainingClient.cpp
)

target_include_directories(training_client_wrapper
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/Include"
)

target_compile_definitions(training_client_wrapper
    PRIVATE
        TRAINING_LIBRARY_PATH="$<TARGET_FILE:training>"
)

target_link_libraries(training_client_wrapper
    PUBLIC
        training_common
    PRIVATE
        ${CMAKE_DL_LIBS}
)

add_dependencies(training_client_wrapper training)

add_executable(training_client
    app/client_main.cpp
)

target_link_libraries(training_client
    PRIVATE
        training_client_wrapper
)

add_executable(client_wrapper_smoke
    tests/client_wrapper_smoke.cpp
)

target_link_libraries(client_wrapper_smoke
    PRIVATE
        training_client_wrapper
)

add_dependencies(client_wrapper_smoke training server)

add_executable(file_transfer_smoke
    tests/file_transfer_smoke.cpp
)

target_link_libraries(file_transfer_smoke
    PRIVATE
        training_client_wrapper
)

target_compile_definitions(file_transfer_smoke
    PRIVATE
        TRAINING_SERVER_DIR="$<TARGET_FILE_DIR:server>"
)

add_dependencies(file_transfer_smoke training server)

add_executable(download_file_smoke
    tests/download_file_smoke.cpp
)

target_link_libraries(download_file_smoke
    PRIVATE
        training_client_wrapper
)

target_compile_definitions(download_file_smoke
    PRIVATE
        TRAINING_SERVER_DIR="$<TARGET_FILE_DIR:server>"
)

add_dependencies(download_file_smoke training server)

add_executable(download_missing_file_smoke
    tests/download_missing_file_smoke.cpp
)

target_link_libraries(download_missing_file_smoke
    PRIVATE
        training_client_wrapper
)

add_dependencies(download_missing_file_smoke training server)

add_executable(download_overwrite_smoke
    tests/download_overwrite_smoke.cpp
)

target_link_libraries(download_overwrite_smoke
    PRIVATE
        training_client_wrapper
)

target_compile_definitions(download_overwrite_smoke
    PRIVATE
        TRAINING_SERVER_DIR="$<TARGET_FILE_DIR:server>"
)

add_dependencies(download_overwrite_smoke training server)

add_executable(send_file_once
    tests/send_file_once.cpp
)

target_link_libraries(send_file_once
    PRIVATE
        training_client_wrapper
)

add_dependencies(send_file_once training server)

add_executable(concurrent_file_transfer_smoke
    tests/concurrent_file_transfer_smoke.cpp
)

target_link_libraries(concurrent_file_transfer_smoke
    PRIVATE
        training_client_wrapper
)

target_compile_definitions(concurrent_file_transfer_smoke
    PRIVATE
        TRAINING_SERVER_DIR="$<TARGET_FILE_DIR:server>"
        SEND_FILE_ONCE_PATH="$<TARGET_FILE:send_file_once>"
)

add_dependencies(concurrent_file_transfer_smoke training server send_file_once)

add_executable(inheritable_client_smoke
    tests/inheritable_client_smoke.cpp
)

target_link_libraries(inheritable_client_smoke
    PRIVATE
        training_client_wrapper
)

add_dependencies(inheritable_client_smoke training server)

add_executable(upload_timeout_recovery_smoke
    tests/upload_timeout_recovery_smoke.cpp
)

target_link_libraries(upload_timeout_recovery_smoke
    PRIVATE
        training_common
        training_utils
        ${CMAKE_DL_LIBS}
)

target_compile_definitions(upload_timeout_recovery_smoke
    PRIVATE
        TRAINING_SERVER_DIR="$<TARGET_FILE_DIR:server>"
        TRAINING_LIBRARY_PATH="$<TARGET_FILE:training>"
)

add_dependencies(upload_timeout_recovery_smoke training server)

add_executable(training_library_api_types
    tests/training_library_api_types.cpp
)

target_link_libraries(training_library_api_types
    PRIVATE
        training_common
)

add_test(
    NAME client_wrapper_roundtrip
    COMMAND
        dbus-run-session
        -- bash -lc
        "$<TARGET_FILE:server> > /tmp/client_wrapper_roundtrip_server.log 2>&1 & server_pid=\$!; sleep 1; $<TARGET_FILE:client_wrapper_smoke>; status=\$?; kill \$server_pid; wait \$server_pid 2>/dev/null; exit \$status"
)

add_test(
    NAME file_transfer_roundtrip
    COMMAND
        dbus-run-session
        -- bash -lc
        "server_dir=\$(dirname \"$<TARGET_FILE:server>\"); rm -rf \"\$server_dir\"/file; mkdir -p \"\$server_dir\"/file; $<TARGET_FILE:server> > /tmp/file_transfer_roundtrip_server.log 2>&1 & server_pid=\$!; sleep 1; $<TARGET_FILE:file_transfer_smoke>; status=\$?; kill \$server_pid; wait \$server_pid 2>/dev/null; rm -rf \"\$server_dir\"/file; exit \$status"
)

add_test(
    NAME concurrent_file_transfer_roundtrip
    COMMAND
        dbus-run-session
        -- bash -lc
        "server_dir=\$(dirname \"$<TARGET_FILE:server>\"); rm -rf \"\$server_dir\"/file; mkdir -p \"\$server_dir\"/file; $<TARGET_FILE:server> > /tmp/concurrent_file_transfer_server.log 2>&1 & server_pid=\$!; sleep 1; $<TARGET_FILE:concurrent_file_transfer_smoke>; status=\$?; kill \$server_pid; wait \$server_pid 2>/dev/null; rm -rf \"\$server_dir\"/file; exit \$status"
)

add_test(
    NAME download_file_roundtrip
    COMMAND
        dbus-run-session
        -- bash -lc
        "server_dir=\$(dirname \"$<TARGET_FILE:server>\"); rm -rf \"\$server_dir\"/file; mkdir -p \"\$server_dir\"/file; $<TARGET_FILE:server> > /tmp/download_file_roundtrip_server.log 2>&1 & server_pid=\$!; sleep 1; $<TARGET_FILE:download_file_smoke>; status=\$?; kill \$server_pid; wait \$server_pid 2>/dev/null; rm -rf \"\$server_dir\"/file; exit \$status"
)

add_test(
    NAME download_missing_file_roundtrip
    COMMAND
        dbus-run-session
        -- bash -lc
        "server_dir=\$(dirname \"$<TARGET_FILE:server>\"); rm -rf \"\$server_dir\"/file; mkdir -p \"\$server_dir\"/file; $<TARGET_FILE:server> > /tmp/download_missing_file_server.log 2>&1 & server_pid=\$!; sleep 1; $<TARGET_FILE:download_missing_file_smoke>; status=\$?; kill \$server_pid; wait \$server_pid 2>/dev/null; rm -rf \"\$server_dir\"/file; exit \$status"
)

add_test(
    NAME download_overwrite_roundtrip
    COMMAND
        dbus-run-session
        -- bash -lc
        "server_dir=\$(dirname \"$<TARGET_FILE:server>\"); rm -rf \"\$server_dir\"/file; mkdir -p \"\$server_dir\"/file; $<TARGET_FILE:server> > /tmp/download_overwrite_server.log 2>&1 & server_pid=\$!; sleep 1; $<TARGET_FILE:download_overwrite_smoke>; status=\$?; kill \$server_pid; wait \$server_pid 2>/dev/null; rm -rf \"\$server_dir\"/file; exit \$status"
)

add_test(
    NAME upload_timeout_recovery_roundtrip
    COMMAND
        dbus-run-session
        -- bash -lc
        "server_dir=\$(dirname \"$<TARGET_FILE:server>\"); rm -rf \"\$server_dir\"/file; mkdir -p \"\$server_dir\"/file; TRAINING_TRANSFER_TIMEOUT_MS=200 $<TARGET_FILE:server> > /tmp/upload_timeout_recovery_server.log 2>&1 & server_pid=\$!; sleep 1; $<TARGET_FILE:upload_timeout_recovery_smoke>; status=\$?; kill \$server_pid; wait \$server_pid 2>/dev/null; rm -rf \"\$server_dir\"/file; exit \$status"
)

add_test(
    NAME inheritable_client_roundtrip
    COMMAND
        dbus-run-session
        -- bash -lc
        "$<TARGET_FILE:server> > /tmp/inheritable_client_server.log 2>&1 & server_pid=\$!; sleep 1; $<TARGET_FILE:inheritable_client_smoke>; status=\$?; kill \$server_pid; wait \$server_pid 2>/dev/null; exit \$status"
)
