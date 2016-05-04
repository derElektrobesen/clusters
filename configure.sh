#!/bin/bash

CLANG_PATH=/Users/p.bereznoy/projects/clang/build/bin
export PATH="$CLANG_PATH:$PATH"
CC="$CLANG_PATH/clang"
CXX="$CLANG_PATH/clang"

aclocal
autoconf
automake --add-missing
automake
./configure --enable-debug=yes
