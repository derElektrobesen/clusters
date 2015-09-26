#!/bin/bash

aclocal
autoconf
automake --add-missing
automake
./configure
