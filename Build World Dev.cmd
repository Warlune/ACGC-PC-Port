@echo off
setlocal
if not defined ACO_TOOLCHAIN set "ACO_TOOLCHAIN=%~dp0..\toolchain\msys64\mingw32"
set "PATH=%ACO_TOOLCHAIN%\bin;%PATH%"
cmake -S "%~dp0pc" -B "%~dp0pc\build-east32" -G "MinGW Makefiles" "-DCMAKE_C_COMPILER=%ACO_TOOLCHAIN%\bin\gcc.exe" "-DCMAKE_CXX_COMPILER=%ACO_TOOLCHAIN%\bin\g++.exe" "-DSDL2_DIR=%ACO_TOOLCHAIN%\lib\cmake\SDL2" -DACO_DEV_EAST=ON
if errorlevel 1 exit /b 1
cmake --build "%~dp0pc\build-east32" --parallel %NUMBER_OF_PROCESSORS% --target ac_pc
if errorlevel 1 exit /b 1
echo Experimental executable: pc\build-east32\bin\AnimalCrossing.exe
echo Requires converted isolated triple-town save and --dev-world.
gcc -std=gnu11 -O2 -fno-strict-aliasing -fwrapv -DTARGET_PC -DBUGFIXES -DF3DEX_GBI_2 -DVERSION=0 -D_LANGUAGE_C -I"%~dp0include" -I"%~dp0src" -I"%~dp0pc\include" -I"%~dp0" "%~dp0pc\tools\generate_dev_town.c" "%~dp0src\game\m_random_field_ovl.c" "%~dp0src\data\combi\data_combi.c" "%~dp0src\static\libc64\qrand.c" -static-libgcc -o "%~dp0pc\build-east32\generate-dev-town.exe"
if errorlevel 1 exit /b 1
echo Native generator tool: pc\build-east32\generate-dev-town.exe SEED
