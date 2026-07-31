# Directory name must match with constant values defined in the SkyConnectManager.cpp
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/Connect)
message(STATUS "Output connect plugin directory=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/PathCreator)
if(PLATFORM_IS_WINDOWS)
    find_package(SimConnect)
    if(SimConnect_FOUND)
        add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/MSFSSimConnectPlugin)
    elseif(SKY_REQUIRE_SIMCONNECT)
        message(FATAL_ERROR
            "SimConnect was not found but SKY_REQUIRE_SIMCONNECT is ON: refusing to produce a "
            "Sky Dolly build that cannot connect to the simulator. See cmake/FindSimConnect.cmake.")
    else()
        message(STATUS "SimConnect not found, skipping MSFSSimConnectPlugin")
    endif()
endif()
