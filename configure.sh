#!/bin/bash

export PATH="/Users/p.bereznoy/projects/clang/build/bin:$PATH"
CC="clang"
CXX="clang"

aclocal
autoconf
automake --add-missing
automake
./configure --enable-debug=yes
