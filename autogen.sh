#!/bin/sh
mkdir -p m4
touch NEWS README AUTHORS ChangeLog
autoreconf --install
automake --add-missing > /dev/null 2>&1
mkdir -p build
