#!/usr/bin/env bash

set +e

"$PROGRAMFILES\(X86\)$\Microsoft Visual Studio 17.0\VC\vcvarsall.bat" amd64

./compile_cl_debug.bat
./build/test_debug.exe
./compile_cl.bat
./build/test.exe
