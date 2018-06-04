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

#ifndef PHP_VAPOR_H
#define PHP_VAPOR_H

extern zend_module_entry vapor_module_entry;
#define phpext_vapor_ptr &vapor_module_entry

#ifdef PHP_WIN32
#define PHP_VAPOR_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#define PHP_VAPOR_API __attribute__((visibility("default")))
#else
#define PHP_VAPOR_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#if defined(ZTS) && defined(COMPILE_DL_VAPOR)
	ZEND_TSRMLS_CACHE_EXTERN()
#endif

#define VAPOR_VERSION       "0.1.0-dev"
#define VAPOR_MAX_FOLDERS   64
#define VAPOR_MAX_SECTIONS  256
#define VAPOR_MAX_FUNCTIONS 256

typedef struct _vapor_tpl vapor_tpl;

struct _vapor_tpl {
    char *basepath;
    char *filename;
    char *filepath;
    char *extension;
    char *layout;
    HashTable *folders;
    HashTable *sections;
    HashTable *functions;
    zend_object std;
};

static inline vapor_tpl *php_vapor_fetch_object(zend_object *obj) {
    return (vapor_tpl *)((char *)(obj) - XtOffsetOf(vapor_tpl, std));
}

#define Z_VAPOR_P(zv) php_vapor_fetch_object(Z_OBJ_P(zv))

#define GetThis() ((Z_TYPE(EX(This)) == IS_OBJECT) ? &EX(This) : NULL)
#define this_ptr  GetThis()
#define vapor_set_value(name, val, copy) \
    if (vapor->name) {                   \
        efree(vapor->name);              \
    }                                    \
    if (copy) {                          \
        vapor->name = estrdup(val);      \
    } else {                             \
        vapor->name = val;               \
    }

#define vapor_set_null(name) \
    if (vapor->name) {       \
        efree(vapor->name);  \
        vapor->name = NULL;  \
    }

ZEND_BEGIN_MODULE_GLOBALS(vapor)
    char *path;
    char *extension;
ZEND_END_MODULE_GLOBALS(vapor)

#define VAPOR_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(vapor, v)

#endif /* PHP_VAPOR_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */