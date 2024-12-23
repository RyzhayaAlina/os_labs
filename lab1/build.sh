#!/bin/bash

mkdir build & cd build
cmake ../hello_world
cmake -- build .
./hello_world