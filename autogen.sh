#!/bin/sh
mkdir -p m4
touch NEWS README AUTHORS ChangeLog
git archive --format=tar --remote=ssh://git@excelforejp.com:4510/utilities/xl4licenses \
    master COPYING | tar x
autoreconf --install
automake --add-missing > /dev/null 2>&1
mkdir -p build
