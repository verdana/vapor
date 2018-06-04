#!/bin/sh

export ZEND_DONT_UNLOAD_MODULES=1
export USE_ZEND_ALLOC=0

cd ../vapor-demo/
valgrind                        \
    --leak-check=full           \
    --show-reachable=no         \
    --track-origins=yes         \
    php -d extension=vapor.so   \
    index.php

