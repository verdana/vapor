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
#include "vapor_int.h"

#define VAPOR_TEMPLATE_GET_OBJ vapor_template *vapor = Z_VAPOR_TEMPLATE_P(getThis());

static zend_object *vapor_object_new_template(zend_class_entry *ce)
{
    vapor_template *template;

    template = ecalloc(1, sizeof(vapor_template) + zend_object_properties_size(ce));

    zend_object_std_init(&template->std, ce);
    object_properties_init(&template->std, ce);
    template->std.handlers = &vapor_object_handlers_template;

    return &template->std;
}

static void vapor_object_free_template(zend_object *obj)
{
    vapor_template *template = php_vapor_template_from_obj(obj);
    zend_object_std_dtor(&template->std);
}

static zval *vapor_object_get_property_template(zval *object, zval *member, int type, void **cache_slot, zval *rv)
{
    zval tmp_member;
    zval *retval = NULL;

    if (Z_TYPE_P(member) != IS_STRING) {
        tmp_member = *member;
        zval_copy_ctor(&tmp_member);
        convert_to_string(&tmp_member);
        member = &tmp_member;
    }

    retval = std_object_handlers.read_property(object, member, type, cache_slot, rv);

    if (member == &tmp_member) {
        zval_dtor(member);
    }

    return retval;
}

static void vapor_object_set_property_template(zval *object, zval *key, zval *value, void **cache_slot)
{
}


PHP_METHOD(Template, __construct)
{
    char *filename;
    size_t len;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(filename, len)
    ZEND_PARSE_PARAMETERS_END();

    php_printf("template = %s\n", filename);
}

PHP_METHOD(Template, make)
{

}


ZEND_BEGIN_ARG_INFO_EX(arginfo__construct, 0, 0, 1)
    ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_make, 0, 0, 1)
    ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO()

const zend_function_entry vapor_methods_template[] = {
    PHP_ME(Template,       __construct,         arginfo__construct,     ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(Template,       make,                arginfo_make,           ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void vapor_template_init()
{
    zend_class_entry ce_template;

    // Class Vapor\Template
    INIT_NS_CLASS_ENTRY(ce_template, "Vapor", "Template", vapor_methods_template);
    ce_template.create_object = vapor_object_new_template;
    vapor_ce_template = zend_register_internal_class(&ce_template);
    memcpy(&vapor_object_handlers_template, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    vapor_object_handlers_template.offset = XtOffsetOf(vapor_template, std);
    vapor_object_handlers_template.free_obj = vapor_object_free_template;
    vapor_object_handlers_template.read_property = vapor_object_get_property_template;
    vapor_object_handlers_template.write_property = vapor_object_set_property_template;
}

