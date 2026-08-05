set(LIBRARY_NAME "Remote")

qt_add_library(${LIBRARY_NAME} SHARED)
add_library(Sky::${LIBRARY_NAME} ALIAS ${LIBRARY_NAME})

target_compile_definitions(${LIBRARY_NAME}
    PRIVATE
        REMOTE_EXPORT
)
target_sources(${LIBRARY_NAME}
    PRIVATE
        include/Remote/RemoteLib.h
        include/Remote/HttpRequest.h src/HttpRequest.cpp
        include/Remote/ApiHandler.h src/ApiHandler.cpp
        include/Remote/RemoteServer.h src/RemoteServer.cpp
        res/Panel.qrc
)
target_include_directories(${LIBRARY_NAME}
    INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
    PRIVATE
        include/Remote
)
target_link_libraries(${LIBRARY_NAME}
    PUBLIC
        Qt6::Core
    PRIVATE
        Qt6::Network
        Sky::Kernel
        Sky::Model
        Sky::Persistence
        Sky::PluginManager
)
set_target_properties(${LIBRARY_NAME}
    PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
)

# --install-addon looks for the MSFS package at Community/skydolly-panel next to the executable,
# which is the layout the release package ships. Mirror it in the build tree, so that installing
# the add-on from a local build works exactly as it does from a downloaded package instead of
# failing with "the panel package is missing".
#
# The staging directory is emptied first, because copy_directory only ever adds: a file deleted from
# the source package would survive in the build tree, get installed into the Community folder, and
# stay there. That is not hypothetical - it is how an obsolete panel descriptor outlived its own
# deletion and had to be cleaned out of a real installation by hand. layout.json lists every file
# the package contains and the simulator checks it, so a stale extra file is not harmless.
add_custom_target(SkyDollyPanelPackage ALL
    COMMAND ${CMAKE_COMMAND} -E rm -rf
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Community/skydolly-panel"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/msfs/skydolly-panel"
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Community/skydolly-panel"
    COMMENT "Staging the MSFS panel package into the build tree"
    VERBATIM
)
add_dependencies(${LIBRARY_NAME} SkyDollyPanelPackage)
