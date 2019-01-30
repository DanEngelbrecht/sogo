@echo off

call .\find_mvsc.bat

if NOT DEFINED VCINSTALLDIR (
    echo "No Visual Studio installation found, aborting, try running run vcvarsall.bat first!"
    exit 1
)

IF NOT EXIST build (
    mkdir build
)

pushd build

cl.exe /nologo /Zi /D_CRT_SECURE_NO_WARNINGS /D_HAS_EXCEPTIONS=0 /EHsc /W4 ..\src\sogo.cpp ..\src\sogo_utils.cpp ..\src\sogo_nodes.cpp ..\third-party\xxHash-1\xxhash.c ..\test/main.cpp /link  /out:test_debug.exe /pdb:test_debug.pdb

popd

exit /B %ERRORLEVEL%

