echo off

if NOT DEFINED VCINSTALLDIR (
    echo "No compatible visual studio found! run vcvarsall.bat first!"
)

IF NOT EXIST build (
    mkdir build
)

cl.exe /nologo /c /Zi /O2 /D_CRT_SECURE_NO_WARNINGS /D_HAS_EXCEPTIONS=0 /EHsc /W4 /Fo.\build\sogo.obj src\sogo.cpp
cl.exe /nologo /c /Zi /O2 /D_CRT_SECURE_NO_WARNINGS /D_HAS_EXCEPTIONS=0 /EHsc /W4 /Fo.\build\sogo_nodes.obj src\sogo_nodes.cpp
cl.exe /nologo /c /Zi /O2 /D_CRT_SECURE_NO_WARNINGS /D_HAS_EXCEPTIONS=0 /EHsc /W4 /Fo.\build\xxhash.obj third-party\xxHash-1\xxhash.c
cl.exe /nologo /Zi /O2 /D_CRT_SECURE_NO_WARNINGS /D_HAS_EXCEPTIONS=0 /EHsc /W4 /Isrc test/main.cpp /Fo.\build\main.obj /link .\build\sogo.obj .\build\sogo_nodes.obj .\build\xxhash.obj /out:.\build\test.exe /pdb:.\build\test.pdb
