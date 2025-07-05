@echo off
echo Installing vcpkg dependencies...
vcpkg install --triplet x64-windows

echo Setting up Visual Studio environment...
set "VSCMD_START_DIR=%~dp0"
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

echo Building project...
msbuild "%~dp0NativeScrollingScreenshot.sln" /property:GenerateFullPaths=true /t:build /consoleloggerparameters:NoSummary /p:Configuration=Debug /p:Platform=x64

echo Copying OpenCV DLLs to output directory...
if exist "%~dp0vcpkg_installed\x64-windows\debug\bin\*.dll" (
    copy "%~dp0vcpkg_installed\x64-windows\debug\bin\*.dll" "%~dp0x64\Debug\" /Y
    echo OpenCV DLLs copied successfully.
) else (
    echo Warning: OpenCV DLLs not found in vcpkg_installed directory.
)

echo Build process completed.
