# SimConnect (in-tree copy)

This folder holds a copy of the SimConnect client library from the **Microsoft Flight Simulator
2024 SDK**. Sky Dolly links against it to talk to the simulator.

It exists so that a build machine without the MSFS SDK installed — in particular the GitHub Actions
Windows runner — can still build the MSFS connect plugin. `cmake/FindSimConnect.cmake` prefers a
locally installed SDK and only falls back to this copy.

## Expected layout

```
3rdParty/SimConnect/
├── include/
│   └── SimConnect.h
└── lib/
    ├── SimConnect.lib
    └── SimConnect.dll
```

If any of the three files is missing, CMake skips the MSFS connect plugin and prints a warning
(or fails outright when `SKY_REQUIRE_SIMCONNECT=ON`).

## How to populate it

1. In MSFS 2024, enable **Options → General → Developers → Developer Mode**.
2. In the developer menu bar: **Help → SDK Installer**, and install the SDK
   (default location `C:\MSFS 2024 SDK`).
3. Copy the three files out of `C:\MSFS 2024 SDK\SimConnect SDK\`:

   | From | To |
   | --- | --- |
   | `include\SimConnect.h` | `3rdParty/SimConnect/include/SimConnect.h` |
   | `lib\SimConnect.lib` | `3rdParty/SimConnect/lib/SimConnect.lib` |
   | `lib\SimConnect.dll` | `3rdParty/SimConnect/lib/SimConnect.dll` |

4. Record the SDK version you copied in `VERSION.txt` next to this file, so it is obvious later
   which SDK build the checked-in library came from.

Note that `.gitignore` excludes `*.dll` globally and carries an explicit exception for
`3rdParty/SimConnect/lib/SimConnect.dll`.

## Do not use the MSFS 2020 SDK here

A module compiled against the legacy 2020 SDK is detected by MSFS 2024 as a *2020 module* and runs
in compatibility mode without the 2024 feature set. The build still succeeds and `FindSimConnect`
will warn, but this is not what Sky Dolly targets.

## Licence

SimConnect is distributed by Microsoft / Asobo Studio under the MIT licence as part of the MSFS
SDK, and the client DLL is redistributable — Sky Dolly has always shipped `SimConnect.dll` in its
releases. See [`THIRD_PARTY.md`](../../THIRD_PARTY.md).
