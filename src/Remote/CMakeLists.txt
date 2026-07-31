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
