#!/bin/bash

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DLLVM_ROOT=/usr/lib/llvm-12 . && cd build && ninja
