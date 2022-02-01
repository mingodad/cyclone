#!/bin/sh
MYCC="gcc -m32 -g"
MYLDFLAGS="-m32 -g -lpthread"
MYBUILD="i686-pc-linux-gnu"
MYTARGET="$MYBUILD"
MYLIB_INSTALL="/home/mingo/dev/c/A_programming-languages/cyclone-dad.git/bin/lib"
#make CC="$MYCC" TARGET_CC="$MYCC" LDFLAGS="$MYLDFLAGS" build="$MYBUILD" target="$MYTARGET" LIB_INSTALL="$MYLIB_INSTALL" $*
make CC="$MYCC" TARGET_CC="$MYCC" LDFLAGS="$MYLDFLAGS" build="$MYBUILD" target="$MYTARGET" $*
#make  LDFLAGS="$MYLDFLAGS" LIB_INSTALL="$MYLIB_INSTALL" $*
