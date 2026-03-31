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

add_test(
    NAME client_wrapper_roundtrip
    COMMAND
        dbus-run-session
        -- bash -lc
        "$<TARGET_FILE:server> > /tmp/client_wrapper_roundtrip_server.log 2>&1 & server_pid=\$!; sleep 1; $<TARGET_FILE:client_wrapper_smoke>; status=\$?; kill \$server_pid; wait \$server_pid 2>/dev/null; exit \$status"
)
