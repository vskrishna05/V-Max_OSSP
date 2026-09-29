#!/usr/bin/env bash
# Quick build script for Linux System Monitor
set -e

echo "Compiling Linux System Monitor (main.c)..."
gcc -Wall -Wextra -O2 -std=gnu11 main.c -o monitor

echo "Build successful! Run './monitor' or 'make run' to launch."
