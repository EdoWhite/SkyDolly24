@echo off
rem Removes the Sky Dolly panel from Microsoft Flight Simulator 2024.
rem
rem Deletes the panel package from the Community folder and removes Sky Dolly's own entry from
rem EXE.xml, leaving every other add-on's entry exactly as it was. Sky Dolly itself is not
rem uninstalled: delete this folder to do that.

setlocal
cd /d "%~dp0"

if not exist "SkyDolly.exe" (
    echo SkyDolly.exe was not found next to this script.
    echo Run Uninstall.bat from the folder Sky Dolly was unpacked into.
    pause
    exit /b 1
)

"SkyDolly.exe" --uninstall-addon
endlocal
