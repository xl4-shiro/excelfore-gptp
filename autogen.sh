#!/bin/sh
touch NEWS AUTHORS ChangeLog
ln -s README.md README
autoreconf --install --no-recursive
automake --add-missing > /dev/null 2>&1
