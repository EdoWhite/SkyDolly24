# SimConnect
#
# Locates the SimConnect client library that Sky Dolly links against.
#
# Sky Dolly targets Microsoft Flight Simulator 2024, so the MSFS 2024 SDK is preferred. The legacy
# MSFS 2020 SDK still builds, but the resulting module is detected by MSFS 2024 as a "2020 module"
# and runs without the 2024 feature set, so it is only a fallback.
#
# Search order:
#   1. SIMCONNECT_ROOT             - cache variable, points directly at a "SimConnect SDK" folder
#   2. MSFS2024_SDK (environment)  - MSFS 2024 SDK install path
#   3. c:/MSFS 2024 SDK/           - MSFS 2024 SDK default install path
#   4. MSFS_SDK (environment)      - MSFS 2020 SDK install path (legacy)
#   5. c:/MSFS SDK/                - MSFS 2020 SDK default install path (legacy)
#   6. 3rdParty/SimConnect/        - copy checked into this repository, used by CI where no SDK
#                                    can be installed (see its README.md)
#
# On success defines the imported target MSFS::SimConnect and sets:
#   SimConnect_FOUND       - TRUE
#   SimConnect_SOURCE      - human readable description of where it was found
#   SimConnect_SDK_VERSION - "2024", "2020" or "unknown"

if(TARGET SimConnect)
    set(SimConnect_FOUND TRUE)
    return()
endif()

set(SimConnect_FOUND FALSE)
set(SimConnect_SOURCE "")
set(SimConnect_SDK_VERSION "unknown")
set(_sc_root "")

# Accepts the first candidate that provides all three files: the header to compile against, the
# import library to link and the DLL to run against. A partial SDK is treated as no SDK.
macro(_sky_try_simconnect_root description root sdk_version)
    if(NOT SimConnect_FOUND AND NOT "${root}" STREQUAL "")
        if(EXISTS "${root}/include/SimConnect.h"
           AND EXISTS "${root}/lib/SimConnect.lib"
           AND EXISTS "${root}/lib/SimConnect.dll")
            set(SimConnect_FOUND TRUE)
            set(SimConnect_SOURCE "${description}")
            set(SimConnect_SDK_VERSION "${sdk_version}")
            set(_sc_root "${root}")
        endif()
    endif()
endmacro()

_sky_try_simconnect_root("SIMCONNECT_ROOT" "${SIMCONNECT_ROOT}" "unknown")
if(DEFINED ENV{MSFS2024_SDK})
    _sky_try_simconnect_root("MSFS2024_SDK environment variable" "$ENV{MSFS2024_SDK}/SimConnect SDK" "2024")
endif()
_sky_try_simconnect_root("MSFS 2024 SDK default location" "c:/MSFS 2024 SDK/SimConnect SDK" "2024")
if(DEFINED ENV{MSFS_SDK})
    _sky_try_simconnect_root("MSFS_SDK environment variable" "$ENV{MSFS_SDK}/SimConnect SDK" "2020")
endif()
_sky_try_simconnect_root("MSFS 2020 SDK default location" "c:/MSFS SDK/SimConnect SDK" "2020")
_sky_try_simconnect_root("in-tree copy" "${CMAKE_SOURCE_DIR}/3rdParty/SimConnect" "unknown")

if(SimConnect_FOUND)
    add_library(SimConnect SHARED IMPORTED)
    add_library(MSFS::SimConnect ALIAS SimConnect)
    set_property(TARGET SimConnect PROPERTY IMPORTED_LOCATION "${_sc_root}/lib/SimConnect.dll")
    set_property(TARGET SimConnect PROPERTY IMPORTED_IMPLIB "${_sc_root}/lib/SimConnect.lib")
    target_include_directories(SimConnect INTERFACE "${_sc_root}/include")

    message(STATUS "SimConnect: found via ${SimConnect_SOURCE}: ${_sc_root}")
    if(SimConnect_SDK_VERSION STREQUAL "2020")
        message(WARNING
            "SimConnect: using the legacy MSFS 2020 SDK. Sky Dolly will be detected by MSFS 2024 "
            "as a 2020 module and run without the 2024 feature set. Install the MSFS 2024 SDK and "
            "set MSFS2024_SDK to build against it.")
    endif()
else()
    message(WARNING
        "SimConnect: not found - the MSFS connect plugin will NOT be built and Sky Dolly will not "
        "be able to talk to the simulator.\n"
        "Install the MSFS 2024 SDK (Developer Mode -> Help -> SDK Installer) and either set the "
        "MSFS2024_SDK environment variable or accept the default install path 'c:/MSFS 2024 SDK'.\n"
        "Alternatively copy SimConnect.h, SimConnect.lib and SimConnect.dll into "
        "'3rdParty/SimConnect/{include,lib}' - see 3rdParty/SimConnect/README.md.")
endif()

unset(_sc_root)
