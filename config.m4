dnl $Id$
dnl config.m4 for extension vapor

PHP_ARG_ENABLE(vapor, whether to enable vapor support,
[  --enable-vapor          Enable vapor template support])
PHP_ARG_ENABLE(vapor-debug, whether to enable debugging support in vapor,
[  --enable-vapor-debug    Enable debugging support in vapor])

if test "$PHP_VAPOR" != "no"; then
    vapor_source="vapor.c"

    dnl Check whether to enable debugging
    if test "$PHP_VAPOR_DEBUG" != "no"; then
        dnl Yes, so set the C macro
        AC_DEFINE(VAPOR_DEBUG, 1, [Include debugging support in vapor])
    fi

    dnl PHP_REQUIRE_CXX()
    dnl PHP_ADD_LIBRARY(stdc++, 1, VAPOR_SHARED_LIBADD)
    dnl CXXFLAGS="$CXXFLAGS -std=c++11"

    PHP_NEW_EXTENSION(vapor, $vapor_source, $ext_shared)
    PHP_SUBST(VAPOR_SHARED_LIBADD)
fi
