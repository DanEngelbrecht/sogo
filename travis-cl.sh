#!/usr/bin/env bash

set +e

choco install visualstudio2017buildtools
echo "%ProgramFiles(x86)%\Microsoft Visual Studio 17.0\VC\vcvarsall.bat" amd64

./compile_clang_debug.sh
./build/test_debug.exe
./compile_clang.sh
./build/test.exe
