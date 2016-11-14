set(SRCS
            tests.cpp
            core/file_sys/path_parser.cpp
            )

set(HEADERS
            )

create_directory_groups(${SRCS} ${HEADERS})

include_directories(../../externals/catch/single_include/)

add_executable(tests ${SRCS} ${HEADERS})
target_link_libraries(tests audio_core common core input_core video_core)
target_link_libraries(tests audio_core common core input_core video_core ${PLATFORM_LIBRARIES})

add_test(NAME tests COMMAND $<TARGET_FILE:tests core>)
