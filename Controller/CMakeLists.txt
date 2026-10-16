file(GLOB_RECURSE PRIVATE_SOURCES CONFIGURE_DEPENDS "*.cpp")

add_library(Controller STATIC
    ${PRIVATE_SOURCES}
)

target_include_directories(Controller
    PUBLIC
        ${CMAKE_SOURCE_DIR}
)

if (TARGET SDL3::SDL3-static)
    target_link_libraries(Controller
        PRIVATE
            SDL3::SDL3-static
    )

    target_compile_definitions(Controller
        PRIVATE
            CONTROLLER_HAS_SDL=1
    )
endif ()
