add_executable(server
    service_main.cpp
)

target_link_libraries(server
    PRIVATE
        training_server_core
)
