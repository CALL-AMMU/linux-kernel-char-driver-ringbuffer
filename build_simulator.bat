@echo off
echo ==============================================================
echo Building Linux Kernel Char Driver Ring Buffer Simulator
echo Author: Amit Bakale (Avionics / Embedded Software Engineer)
echo ==============================================================
gcc -O2 -Wall user\user_simulator.c -o ringbuf_test.exe
if %ERRORLEVEL% EQU 0 (
    echo [SUCCESS] Build completed successfully: ringbuf_test.exe
    echo Run ringbuf_test.exe to execute multi-producer multi-consumer verification!
) else (
    echo [FAILED] Compilation failed with error %ERRORLEVEL%
)
