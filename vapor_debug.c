/*
  +----------------------------------------------------------------------+
  | The Vapor Template Engine                                            |
  +----------------------------------------------------------------------+
  | Copyright (c) 2018 Verdana Mu                                        |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author: Verdana Mu <verdana.cn@gmail.com>                            |
  +----------------------------------------------------------------------+
*/

/* $Id$ */

#include "php.h"
#include "vapor.h"
#include "ext/standard/php_var.h"

void zend_array_dump(zend_array *data)
{
    if (data != NULL) {
        zval tmp;
        ZVAL_ARR(&tmp, data);
        php_var_dump(&tmp, 0);
    }
}

void vapor_engine_dump(vapor_engine *val)
{
    php_printf("[object vapor_engine] = {\n\
    basepath  = %s\n\
    extension = %s\n\
    exception = %d\n\
}\n\n",
    val->basepath,
    val->extension,
    val->exception);
}

void vapor_template_dump(vapor_template *val)
{
    php_printf("[object vapor_template] = {\n\
    initialized = %d\n\
    folder      = %s\n\
    basename    = %s\n\
    filepath    = %s\n\
    layout_name = %s\n\
}\n\n",
    val->initialized,
    val->folder,
    val->basename,
    val->filepath,
    val->layout_name);
}
