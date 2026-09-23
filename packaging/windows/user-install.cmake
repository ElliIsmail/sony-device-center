# Run by the `user-install` target after a Release build: refreshes the
# per-user copy in PREFIX so the login autostart entry always launches the
# latest build with its Qt runtime beside it.
#
# Expects -DBUILD_DIR, -DPREFIX, -DCONFIG and -DSTAMP.

if(NOT CONFIG STREQUAL "Release")
    return()
endif()

file(TO_NATIVE_PATH "${PREFIX}/bin/sony-device-center.exe" _exe)

# The running tray app locks its .exe and DLLs, so the install would fail
# half-way. Stop that one copy (a dev-tree instance is left alone) and
# relaunch it once the new files are in place. The lookup goes through WMI
# because Get-Process leaves .Path empty for a 64-bit process when this runs
# under a 32-bit cmake, such as the one bundled with the VS Build Tools.
execute_process(
    COMMAND powershell -NoProfile -NonInteractive -Command
        "$p = Get-CimInstance Win32_Process -Filter \"Name='sony-device-center.exe'\" | Where-Object { $_.ExecutablePath -eq '${_exe}' }; if ($p) { $p | ForEach-Object { Stop-Process -Id $_.ProcessId -Force; Wait-Process -Id $_.ProcessId -Timeout 10 -ErrorAction SilentlyContinue }; exit 3 }; exit 0"
    RESULT_VARIABLE _stopped)

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${BUILD_DIR}" --config "${CONFIG}" --prefix "${PREFIX}"
    RESULT_VARIABLE _rc
    OUTPUT_QUIET)

if(_stopped EQUAL 3)
    execute_process(COMMAND powershell -NoProfile -NonInteractive -Command
        "Start-Process -FilePath '${_exe}'")
endif()

if(NOT _rc EQUAL 0)
    # Leaving the stamp untouched makes the next build try again.
    message(WARNING "Per-user install to ${PREFIX} failed (exit ${_rc}).")
    return()
endif()

message(STATUS "Installed to ${PREFIX}")
file(TOUCH "${STAMP}")
