#!/bin/bash

mkdir build
cd build
cmake ../process_manager
cmake --build .
./test_process_manager