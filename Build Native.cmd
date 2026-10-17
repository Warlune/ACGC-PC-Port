@echo off
setlocal
if not defined ACO_TOOLCHAIN set "ACO_TOOLCHAIN=%~dp0..\toolchain\msys64\mingw32"
if not exist "%ACO_TOOLCHAIN%\bin\gcc.exe" (
  echo Set ACO_TOOLCHAIN to an MSYS2 mingw32 directory. See README.md.
  exit /b 1
)
set "PATH=%ACO_TOOLCHAIN%\bin;%PATH%"
cmake -S "%~dp0pc" -B "%~dp0pc\build32" -G "MinGW Makefiles" "-DCMAKE_C_COMPILER=%ACO_TOOLCHAIN%\bin\gcc.exe" "-DCMAKE_CXX_COMPILER=%ACO_TOOLCHAIN%\bin\g++.exe" "-DSDL2_DIR=%ACO_TOOLCHAIN%\lib\cmake\SDL2"
if errorlevel 1 exit /b 1
cmake --build "%~dp0pc\build32" --parallel %NUMBER_OF_PROCESSORS%
if errorlevel 1 exit /b 1
echo Built pc\build32\bin\AnimalCrossing.exe
