#!/bin/sh

make distclean
phpize

./configure --enable-vapor --enable-vapor-debug
make
sudo make install
