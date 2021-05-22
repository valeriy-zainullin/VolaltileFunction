#!/bin/bash

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS='-fsanitize=address' -DLLVM_ROOT=/usr/lib/llvm-12 . && cd build && ninja
