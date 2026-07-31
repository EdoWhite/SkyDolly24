@echo off
rem Installs the Sky Dolly panel into Microsoft Flight Simulator 2024.
rem
rem Copies the toolbar panel into the simulator's Community folder and adds an entry to EXE.xml, so
rem that the simulator starts Sky Dolly by itself. EXE.xml is backed up first, and only Sky Dolly's
rem own entry is ever touched: other add-ons are left alone.
rem
rem Run this from the folder it came in, next to SkyDolly.exe. No administrator rights are needed:
rem everything it writes is below the current user's own profile.

setlocal
cd /d "%~dp0"

if not exist "SkyDolly.exe" (
    echo SkyDolly.exe was not found next to this script.
    echo Run Install.bat from the folder Sky Dolly was unpacked into.
    pause
    exit /b 1
)

"SkyDolly.exe" --install-addon
endlocal
