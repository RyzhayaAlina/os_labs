@echo off

cd process_manager
mkdir build & cd build
cmake -G "MinGW Makefiles" ..
cmake --build .
test_process_manager.exe