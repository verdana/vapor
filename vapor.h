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

typedef struct _vapor_engine    vapor_engine;
typedef struct _vapor_template  vapor_template;

struct _vapor_engine {
    char            *basepath;      //   0
    char            *extension;     //   8
    zend_array      *folders;       //  16
    zend_array      *functions;     //  24
    zend_array      *sections;      //  32
    vapor_template  *current;       //  40
    zend_bool        exception;     //
    zend_object      std;           // 104
};

struct _vapor_template {
    zend_bool        initialized;   //
    char            *folder;        //
    char            *basename;      //
    char            *filepath;      //
    char            *layout;        //
    zend_array      *layout_data;   //
    vapor_engine    *engine;
    zend_object      std;
};

static inline vapor_engine *php_vapor_engine_from_obj(zend_object *obj)
{
    return (vapor_engine *)((char *)(obj)-XtOffsetOf(vapor_engine, std));
}
#define Z_VAPOR_ENGINE_P(zv) php_vapor_engine_from_obj(Z_OBJ_P(zv))

static inline vapor_template *php_vapor_template_from_obj(zend_object *obj)
{
    return (vapor_template *)((char *)(obj)-XtOffsetOf(vapor_template, std));
}
#define Z_VAPOR_TEMPLATE_P(zv) php_vapor_template_from_obj(Z_OBJ_P(zv))

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
