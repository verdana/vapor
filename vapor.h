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

#define VAPOR_VERSION           "0.1.0-dev"
#define VAPOR_MAX_FOLDERS       64
#define VAPOR_MAX_SECTIONS      256
#define VAPOR_MAX_FUNCTIONS     256

typedef struct _vapor_core      vapor_core;
typedef struct _vapor_template  vapor_template;

struct _vapor_core {
    char            *basepath;      //   0
    char            *extension;     //   8
    zend_array      *folders;       //  16
    zend_array      *functions;     //  24
    zend_array      *sections;      //  32
    vapor_template  *current;       //  40
    zend_object      std;           // 104
};

struct _vapor_template {
    char            *folder;        //
    char            *basename;      //
    char            *filepath;      //
    vapor_template  *layout;        //
};

static inline vapor_core *php_vapor_fetch_object(zend_object *obj)
{
    return (vapor_core *)((char *)(obj)-XtOffsetOf(vapor_core, std));
}
#define Z_VAPOR_P(zv) php_vapor_fetch_object(Z_OBJ_P(zv))
#define GetThis() ((Z_TYPE(EX(This)) == IS_OBJECT) ? &EX(This) : NULL)

// ZEND_BEGIN_MODULE_GLOBALS(vapor)
//     char *path;
//     char *extension;
// ZEND_END_MODULE_GLOBALS(vapor)

// #define VAPOR_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(vapor, v)

#endif /* PHP_VAPOR_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
