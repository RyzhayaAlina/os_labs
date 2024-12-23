@echo off

cd hello_world
mkdir build & cd build
cmake -G "MinGW Makefiles" ..
cmake --build .
helloworld.exe