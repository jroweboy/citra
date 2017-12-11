if (ENABLE_WEB_SERVICE)
set(SRCS
            announce_multiplayer_session.cpp
            announce_room_json.cpp
            telemetry_json.cpp
            verify_login.cpp
            web_backend.cpp
            )
else()
set(SRCS
            announce_multiplayer_session.cpp
            )
endif()

if (ENABLE_WEB_SERVICE)
set(HEADERS
            announce_multiplayer_session.h
            announce_room_json.h
            telemetry_json.h
            verify_login.h
            web_backend.h
            )

else()
set(HEADERS
            announce_multiplayer_session.h
            )
endif()

create_directory_groups(${SRCS} ${HEADERS})

add_library(web_service STATIC ${SRCS} ${HEADERS})
target_link_libraries(web_service PUBLIC common cpr json-headers network)
