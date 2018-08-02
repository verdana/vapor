#!/bin/sh

export ZEND_DONT_UNLOAD_MODULES=1
export USE_ZEND_ALLOC=0

LOGFILE="`pwd`/valgrind.log"

echo ----------------------------------------------------------------------

cd ../vapor-demo
valgrind                        \
    --leak-check=full           \
    --show-reachable=no         \
    --track-origins=yes         \
    --log-file=$LOGFILE         \
    /usr/local/bin/php          \
        -d extension=vapor.so   \
        -d cli_server.color=on  \
    index.php
