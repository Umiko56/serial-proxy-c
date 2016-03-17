set( CMAKE_C_FLAGS "-ggdb -Wl,-z,relro -D_FORTIFY_SOURCE=2 -O2 -fstack-protector-strong -Wformat -Werror=format-security" )

include_directories(
    .
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}
)

set( SOURCES
    ${PROJECT_SOURCE_DIR}/src/server.c
    ${PROJECT_SOURCE_DIR}/src/serial.c
    ${PROJECT_SOURCE_DIR}/src/config.c
    ${PROJECT_SOURCE_DIR}/src/ini.c
    ${PROJECT_SOURCE_DIR}/src/ae.c
)

add_executable( sproxyd ${SOURCES} )

target_link_libraries( sproxyd -lutil )

install( TARGETS sproxyd RUNTIME DESTINATION usr/sbin )
