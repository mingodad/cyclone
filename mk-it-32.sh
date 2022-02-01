#!/bin/sh
MYCC="gcc -m32 -g"
MYLDFLAGS="-m32 -g -lpthread"
MYBUILD="i686-pc-linux-gnu"
MYTARGET="$MYBUILD"
make CC="$MYCC" TARGET_CC="$MYCC" LDFLAGS="$MYLDFLAGS" build="$MYBUILD" target="$MYTARGET" $*
