#!/usr/bin/sh
cd kernel
echo "\nClean and rebuild melib\n"
make -f Makefile clean
make -f Makefile
cd ..
echo "\nClean and rebuild sample\n"
make -f Makefile clean
make -f Makefile
