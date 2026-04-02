add_executable(server
    service_main.cpp
)

target_link_libraries(server PRIVATE core)

install(
    TARGETS server
    RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}/training"
)
