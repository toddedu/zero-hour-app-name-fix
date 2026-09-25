#!/bin/sh
# Build build/dinput8.dll (32-bit, works on Windows and Proton).
# Needs: sudo apt install gcc-mingw-w64-i686
set -e
cd "$(dirname "$0")"

CC=i686-w64-mingw32-gcc
CFLAGS="-O2 -Wall -ffreestanding -fno-builtin"

mkdir -p build
$CC $CFLAGS -c -o build/appname_fix.o appname_fix.c
$CC $CFLAGS -c -o build/log.o log.c
$CC $CFLAGS -c -o build/dinput8.o dinput8.c
$CC -shared -nostdlib -s -o build/dinput8.dll build/dinput8.o build/appname_fix.o build/log.o dinput8.def \
    -Wl,-e,_DllMain@12 -Wl,--enable-stdcall-fixup -lkernel32
echo "built build/dinput8.dll"
