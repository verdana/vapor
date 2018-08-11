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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/html.h"
#include "ext/standard/info.h"
#include "ext/standard/php_string.h"
#include "zend_exceptions.h"

#include "vapor.h"
#include "vapor_int.h"

//static int le_vapor;

zend_class_entry *vapor_ce_engine;
zend_class_entry *vapor_ce_template;
zend_class_entry *vapor_ce_exception;

zend_object_handlers vapor_object_handlers_engine;
zend_object_handlers vapor_object_handlers_template;

// ZEND_DECLARE_MODULE_GLOBALS(vapor)

/* {{{ vapor_init_globals */
// static void vapor_init_globals(zend_vapor_globals *vapor_globals)
// {
//     memset(vapor_globals, 0, sizeof(zend_vapor_globals));
//     vapor_globals->path = "";
//     vapor_globals->extension = "php";
// }


/* {{{ PHP_INI */
// PHP_INI_BEGIN()
//     STD_PHP_INI_ENTRY("vapor.path", "", PHP_INI_ALL, OnUpdateString, path, zend_vapor_globals, vapor_globals)
//     STD_PHP_INI_ENTRY("vapor.extension", "php", PHP_INI_ALL, OnUpdateString, extension, zend_vapor_globals, vapor_globals)
// PHP_INI_END()


#define VAPOR_ENGINE_GET_OBJ vapor_engine *engine = Z_VAPOR_ENGINE_P(getThis());

void vapor_report_error(vapor_engine *obj, char *format, ...)
{
    char *message;
    va_list arg;

    va_start(arg, format);
    vspprintf(&message, 0, format, arg);
    va_end(arg);

    if (obj && obj->exception) {
        zend_throw_exception(vapor_ce_exception, message, 0);
    } else {
        php_error_docref(NULL, E_WARNING, "%s", message);
    }

    if (message) efree(message);
}

void vapor_data_copy(zend_array *symtable, zend_array *data)
{
    zend_string *key;
    zval *val;

    if (UNEXPECTED(data == NULL)) {
        return;
    }

    ZEND_HASH_FOREACH_STR_KEY_VAL(data, key, val) {
        Z_TRY_ADDREF_P(val);
        zend_hash_str_update(symtable, ZSTR_VAL(key), ZSTR_LEN(key), val);
    } ZEND_HASH_FOREACH_END();
}

static zend_object *vapor_engine_new(zend_class_entry *ce)
{
    vapor_engine *engine;

    engine = ecalloc(1, sizeof(vapor_engine) + zend_object_properties_size(ce));

    zend_object_std_init(&engine->std, ce);
    object_properties_init(&engine->std, ce);
    engine->std.handlers = &vapor_object_handlers_engine;

    return &engine->std;
}

static void vapor_engine_free_storage(zend_object *obj)
{
    vapor_engine *engine = php_vapor_engine_from_obj(obj);

    if (engine->basepath) {
        efree(engine->basepath);
    }
    if (engine->extension) {
        efree(engine->extension);
    }
    if (engine->folders) {
        zend_hash_destroy(engine->folders);
        FREE_HASHTABLE(engine->folders);
    }
    if (engine->functions) {
        zend_hash_destroy(engine->functions);
        FREE_HASHTABLE(engine->functions);
    }
    if (engine->sections) {
        zend_hash_destroy(engine->sections);
        FREE_HASHTABLE(engine->sections);
    }

    zend_object_std_dtor(&engine->std);
}

static zval *vapor_object_get_property_engine(zval *object, zval *member, int type, void **cache_slot, zval *rv)
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

static void vapor_object_set_property_engine(zval *object, zval *key, zval *value, void **cache_slot)
{
    vapor_engine *vapor = Z_VAPOR_ENGINE_P(object);
    convert_to_string(key);

    std_object_handlers.write_property(object, key, value, cache_slot);
}

static int vapor_get_callback(INTERNAL_FUNCTION_PARAMETERS, char *func_name, zval **callback)
{
    VAPOR_ENGINE_GET_OBJ

    if ((*callback = zend_hash_str_find(engine->functions, func_name, strlen(func_name))) != NULL) {
        if (zend_is_callable(*callback, 0, 0)) {
            return SUCCESS;
        }
    }
    return FAILURE;
}

static PHP_METHOD(Engine, __construct)
{
    char *path, *ext = NULL, resolved_path[MAXPATHLEN];
    size_t path_len, ext_len;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(path, path_len)
        Z_PARAM_OPTIONAL
        Z_PARAM_STRING(ext, ext_len)
    ZEND_PARSE_PARAMETERS_END();

    VAPOR_ENGINE_GET_OBJ

    if (!VCWD_REALPATH(path, resolved_path)) {
        vapor_report_error(engine, "Could not resolve file path");
        return;
    }

    engine->basepath = estrdup(resolved_path);
    engine->extension = (ext) ? estrdup(ext): NULL;

    ALLOC_HASHTABLE(engine->folders);
    ALLOC_HASHTABLE(engine->sections);
    ALLOC_HASHTABLE(engine->functions);

    zend_hash_init(engine->folders, 0, NULL, ZVAL_PTR_DTOR, 0);
    zend_hash_init(engine->sections, 0, NULL, ZVAL_PTR_DTOR, 0);
    zend_hash_init(engine->functions, 0, NULL, ZVAL_PTR_DTOR, 0);

    zend_update_property_string(vapor_ce_engine, getThis(), "basepath", strlen("basepath"), engine->basepath);
    if (engine->extension) {
        zend_update_property_string(vapor_ce_engine, getThis(), "extension", strlen("extension"), engine->extension);
    }
}

static PHP_METHOD(Engine, __call)
{
    char *function;
    int argc;
    size_t len;
    zval *callback, *element, *params, *params_arr;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(function, len)
        Z_PARAM_ZVAL(params_arr)
    ZEND_PARSE_PARAMETERS_END();

    argc   = zend_hash_num_elements(Z_ARRVAL_P(params_arr));
    params = safe_emalloc(sizeof(zval), argc, 0);

    argc = 0;
    ZEND_HASH_FOREACH_VAL (Z_ARRVAL_P(params_arr), element) {
        ZVAL_COPY(&params[argc], element);
        argc++;
    } ZEND_HASH_FOREACH_END();

    if (SUCCESS == vapor_get_callback(INTERNAL_FUNCTION_PARAM_PASSTHRU, function, &callback)) {
        zend_fcall_info fci;
        zend_fcall_info_cache fcc;
        zval retval;

        if (SUCCESS == zend_fcall_info_init(callback, 0, &fci, &fcc, NULL, NULL)) {
            fci.retval      = &retval;
            fci.params      = params;
            fci.param_count = argc;

            if (SUCCESS == zend_call_function(&fci, &fcc)) {
                ZVAL_COPY_VALUE(return_value, &retval);
            }
        }
    }

    for (int i = 0; i < argc; i++) {
        zval_ptr_dtor(&params[i]);
    }

    efree(params);
}

static PHP_METHOD(Engine, addFolder)
{
    char *folder, *path;
    char resolved_path[MAXPATHLEN];
    size_t folder_len, path_len;
    zend_bool fallback = 0;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(folder, folder_len)
        Z_PARAM_STRING(path, path_len)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL(fallback)
    ZEND_PARSE_PARAMETERS_END();

    VAPOR_ENGINE_GET_OBJ

    if (!path_len || !VCWD_REALPATH(path, resolved_path)) {
        vapor_report_error(engine, "Could not resolve folder path");
        return;
    }

    zval zv;
    ZVAL_STRING(&zv, resolved_path);
    zend_hash_str_update(engine->folders, folder, folder_len, &zv);
}

static PHP_METHOD(Engine, getFolders)
{
    VAPOR_ENGINE_GET_OBJ
    RETURN_ARR(zend_array_dup(engine->folders));
}

static PHP_METHOD(Engine, setFileExtension)
{
    char *ext;
    size_t ext_len;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(ext, ext_len)
    ZEND_PARSE_PARAMETERS_END();

    VAPOR_ENGINE_GET_OBJ

    if (engine->extension) {
        efree(engine->extension);
    }
    engine->extension = estrdup(ext);

    zend_update_property_string(vapor_ce_engine, getThis(), ZEND_STRL("extension"), engine->extension);
}

static PHP_METHOD(Engine, registerFunction)
{
    char *name;
    size_t len;
    zval *func, tmp;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(name, len)
        Z_PARAM_ZVAL(func)
    ZEND_PARSE_PARAMETERS_END();

    VAPOR_ENGINE_GET_OBJ

    tmp = *func;
    zval_copy_ctor(&tmp);
    zend_hash_str_update(engine->functions, name, len, &tmp);
}

static PHP_METHOD(Engine, dropFunction)
{
    char *name;
    size_t len;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(name, len)
    ZEND_PARSE_PARAMETERS_END();

    VAPOR_ENGINE_GET_OBJ

    zend_hash_str_del(engine->functions, name, len);
}

static PHP_METHOD(Engine, getFunction)
{
    char *name;
    size_t len;
    zval *tmpval = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(name, len)
    ZEND_PARSE_PARAMETERS_END();

    VAPOR_ENGINE_GET_OBJ

    if ((tmpval = zend_hash_str_find(engine->functions, name, len))) {
        ZVAL_ZVAL(return_value, tmpval, 1, 0);    // copy
        tmpval = NULL;
    }
    else {
        RETURN_NULL();
    }
}

static PHP_METHOD(Engine, path)
{
    char *filename = NULL;
    size_t len;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_STRING(filename, len)
    ZEND_PARSE_PARAMETERS_END();

    VAPOR_ENGINE_GET_OBJ
    php_printf(engine->current->filepath);
}

static PHP_METHOD(Engine, layout)
{
    char *layout, *layoutcopy;
    size_t len;
    vapor_template *tpl;
    zend_array *data = NULL;
    zval obj;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(layout, len)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_HT(data)
    ZEND_PARSE_PARAMETERS_END();

    VAPOR_ENGINE_GET_OBJ

    if (UNEXPECTED(engine->current == NULL)) {
        vapor_report_error(engine, "Failed to set layout");
        RETURN_FALSE;
    }

    engine->current->layout = estrdup(layout);
    if (EXPECTED(data != NULL)) {
        engine->current->layout_data = data;
    }

    RETURN_TRUE;
}

static PHP_METHOD(Engine, section)
{
    char *content, *section;
    size_t len;
    zval *tmp;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(section, len)
    ZEND_PARSE_PARAMETERS_END();

    VAPOR_ENGINE_GET_OBJ

    if (NULL != (tmp = zend_hash_str_find(engine->sections, section, len)) && Z_TYPE_P(tmp) == IS_STRING) {
        RETURN_STRING(Z_STRVAL_P(tmp));
    }
    RETURN_NULL();
}

static PHP_METHOD(Engine, insert)
{
    char *tplname, *namecopy;
    size_t len;
    vapor_template *tpl;
    zend_array *data = NULL;
    zval obj;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(tplname, len)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_HT(data)
    ZEND_PARSE_PARAMETERS_END();

    VAPOR_ENGINE_GET_OBJ

    // Make a template
    namecopy = estrdup(tplname);
    vapor_template_instantiate(&obj);
    tpl = Z_VAPOR_TEMPLATE_P(&obj);

    if (!vapor_template_initialize(tpl, engine, namecopy)) {
        zval_ptr_dtor(&obj);
        RETURN_FALSE;
    }

    // Copy user data
    if (EXPECTED(data != NULL)) {
        vapor_data_copy(zend_rebuild_symbol_table(), data);
    }

    // Render template
    zval content;
    ZVAL_NULL(&content);
    vapor_template_render(engine, tpl, NULL, &content);
    php_printf("%s", Z_STRVAL(content));

    // free
    zval_ptr_dtor(&obj);
    zval_ptr_dtor(&content);
    efree(namecopy);
}

static PHP_METHOD(Engine, escape)
{
    char *str = NULL;
    size_t len;
    zend_string *callbacks = NULL, *delim, *escaped;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(str, len)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR(callbacks)
    ZEND_PARSE_PARAMETERS_END();

    escaped = php_escape_html_entities(str, strlen(str), 0, ENT_QUOTES | ENT_SUBSTITUTE, NULL);

    if (callbacks && ZSTR_LEN(callbacks) > 0) {
        zval used_callbacks;
        array_init(&used_callbacks);

        delim = zend_string_init("|", strlen("|"), 0);
        php_explode(delim, callbacks, &used_callbacks, VAPOR_MAX_FUNCTIONS);

        if (&used_callbacks) {
            zval *funcname;
            ZEND_HASH_FOREACH_VAL (Z_ARRVAL(used_callbacks), funcname) {
                zval *callback;
                if (SUCCESS == vapor_get_callback(INTERNAL_FUNCTION_PARAM_PASSTHRU, Z_STRVAL_P(funcname), &callback)) {
                    zend_fcall_info fci;
                    zend_fcall_info_cache fcc;
                    zval argv[1], retval;

                    ZVAL_STR(&argv[0], escaped);
                    if (SUCCESS == zend_fcall_info_init(callback, 0, &fci, &fcc, NULL, NULL)) {
                        fci.retval      = &retval;
                        fci.params      = argv;
                        fci.param_count = 1;

                        if (SUCCESS == zend_call_function(&fci, &fcc)) {
                            if (!Z_ISNULL(retval)) {
                                zend_string_release(escaped);
                                escaped = Z_STR(retval);
                            }
                        }
                    }
                }
            }
            ZEND_HASH_FOREACH_END();
        }

        zval_ptr_dtor(&used_callbacks);
        zend_string_release(delim);
    }

    RETURN_STR(escaped);
}

static PHP_METHOD(Engine, make)
{
    char *tplname, *namecopy;
    size_t len;
    vapor_template *tpl;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(tplname, len)
    ZEND_PARSE_PARAMETERS_END();

    VAPOR_ENGINE_GET_OBJ

    namecopy = estrdup(tplname);

    vapor_template_instantiate(return_value);
    tpl = Z_VAPOR_TEMPLATE_P(return_value);

    if (!vapor_template_initialize(Z_VAPOR_TEMPLATE_P(return_value), engine, namecopy)) {
        zval_ptr_dtor(return_value);
        RETURN_FALSE;
    }

    efree(namecopy);
}

static PHP_METHOD(Engine, render)
{
    char *tplname, *namecopy;
    size_t len;
    vapor_template *tpl;
    zend_array *data = NULL;
    zval obj;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(tplname, len)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_HT(data)
    ZEND_PARSE_PARAMETERS_END();

    VAPOR_ENGINE_GET_OBJ

    // Make a template
    namecopy = estrdup(tplname);
    vapor_template_instantiate(&obj);
    tpl = Z_VAPOR_TEMPLATE_P(&obj);

    if (!vapor_template_initialize(tpl, engine, namecopy)) {
        zval_ptr_dtor(&obj);
        RETURN_FALSE;
    }
    engine->current = tpl;

    // Copy user data
    if (EXPECTED(data != NULL)) {
        vapor_data_copy(zend_rebuild_symbol_table(), data);
    }

    // Render template
    ZVAL_NULL(return_value);
    vapor_template_render(engine, tpl, NULL, return_value);
    zval_ptr_dtor(&obj);
}

/* {{{ arginfo */
ZEND_BEGIN_ARG_INFO_EX(arginfo__construct, 0, 0, 1)
    ZEND_ARG_INFO(0, path)
    ZEND_ARG_INFO(0, extension)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo__call, 0, 0, 2)
    ZEND_ARG_INFO(0, function_name)
    ZEND_ARG_INFO(0, arguments)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_add_folder, 0, 0, 2)
    ZEND_ARG_INFO(0, name)
    ZEND_ARG_INFO(0, folder)
    ZEND_ARG_INFO(0, fallback)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_set_extension, 0, 0, 1)
    ZEND_ARG_INFO(0, extension)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_register_func, 0, 0, 2)
    ZEND_ARG_INFO(0, name)
    ZEND_ARG_INFO(0, userfunc)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_drop_func, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_get_func, 0, 0, 1)
    ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_path, 0, 0, 0)
    ZEND_ARG_INFO(0, filename)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_layout, 0, 0, 1)
    ZEND_ARG_INFO(0, layout)
    ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_section, 0, 0, 1)
    ZEND_ARG_INFO(0, section)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_insert, 0, 0, 1)
    ZEND_ARG_INFO(0, filename)
    ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_escape, 0, 0, 1)
    ZEND_ARG_INFO(0, var)
    ZEND_ARG_INFO(0, funcs)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_make, 0, 0, 1)
    ZEND_ARG_INFO(0, tplname)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_render, 0, 0, 1)
    ZEND_ARG_INFO(0, filename)
    ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()


/* {{{ vapor_methods_engine[] : Vapor\Engine class */
static const zend_function_entry vapor_methods_engine[] = {
    PHP_ME(Engine,      __construct,        arginfo__construct,     ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(Engine,      __call,             arginfo__call,          ZEND_ACC_PUBLIC | ZEND_ACC_FINAL)
    PHP_ME(Engine,      addFolder,          arginfo_add_folder,     ZEND_ACC_PUBLIC)
    PHP_ME(Engine,      getFolders,         NULL,                   ZEND_ACC_PUBLIC)
    PHP_ME(Engine,      setFileExtension,   arginfo_set_extension,  ZEND_ACC_PUBLIC)
    PHP_ME(Engine,      registerFunction,   arginfo_register_func,  ZEND_ACC_PUBLIC)
    PHP_ME(Engine,      dropFunction,       arginfo_drop_func,      ZEND_ACC_PUBLIC)
    PHP_ME(Engine,      getFunction,        arginfo_get_func,       ZEND_ACC_PUBLIC)
    PHP_ME(Engine,      path,               arginfo_path,           ZEND_ACC_PUBLIC)
    PHP_ME(Engine,      layout,             arginfo_layout,         ZEND_ACC_PUBLIC)
    PHP_ME(Engine,      section,            arginfo_section,        ZEND_ACC_PUBLIC)
    PHP_ME(Engine,      insert,             arginfo_insert,         ZEND_ACC_PUBLIC)
    PHP_ME(Engine,      escape,             arginfo_escape,         ZEND_ACC_PUBLIC)
    PHP_ME(Engine,      make,               arginfo_make,           ZEND_ACC_PUBLIC)
    PHP_ME(Engine,      render,             arginfo_render,         ZEND_ACC_PUBLIC)
    PHP_MALIAS(Engine,  e,      escape,     arginfo_escape,         ZEND_ACC_PUBLIC)
    PHP_FE_END
};

/* {{{ vapor_methods_exception[] : Vapor\Exception class */
static const zend_function_entry vapor_methods_exception[] = {
    PHP_FE_END
};

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(vapor)
{
    zend_class_entry ce_engine, ce_template, ce_exception;

    // register resource dtor
    // le_vapor = zend_register_list_destructors_ex(vapor_resource_dtor, NULL, "Vapor Template", module_number);

    // REGISTER_INI_ENTRIES();

    // Register Class Vapor\Engine
    INIT_NS_CLASS_ENTRY(ce_engine, "Vapor", "Engine", vapor_methods_engine);
    ce_engine.create_object = vapor_engine_new;
    vapor_ce_engine = zend_register_internal_class(&ce_engine);
    memcpy(&vapor_object_handlers_engine, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    vapor_object_handlers_engine.offset = XtOffsetOf(vapor_engine, std);
    vapor_object_handlers_engine.free_obj = vapor_engine_free_storage;
    vapor_object_handlers_engine.read_property = vapor_object_get_property_engine;
    vapor_object_handlers_engine.write_property = vapor_object_set_property_engine;
    zend_declare_property_null(vapor_ce_engine, ZEND_STRL("basepath"), ZEND_ACC_PUBLIC);
    zend_declare_property_null(vapor_ce_engine, ZEND_STRL("extension"), ZEND_ACC_PUBLIC);

    // Register Class Vapor\Template
    INIT_NS_CLASS_ENTRY(ce_template, "Vapor", "Template", vapor_methods_template);
    ce_template.create_object = vapor_template_new;
    vapor_ce_template = zend_register_internal_class(&ce_template);
    memcpy(&vapor_object_handlers_template, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    vapor_object_handlers_template.offset = XtOffsetOf(vapor_template, std);
    vapor_object_handlers_template.free_obj = vapor_template_free_storage;
    vapor_object_handlers_template.read_property = vapor_object_get_property_template;
    vapor_object_handlers_template.write_property = vapor_object_set_property_template;

    // Register Class Vapor\Exception
    INIT_NS_CLASS_ENTRY(ce_exception, "Vapor", "Exception", vapor_methods_exception);
    vapor_ce_exception = zend_register_internal_class_ex(&ce_exception, zend_ce_exception);

    return SUCCESS;
}

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(vapor)
{
    // UNREGISTER_INI_ENTRIES();
    return SUCCESS;
}

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(vapor)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "Vapor Support", "enabled");
    php_info_print_table_row(2, "Version", VAPOR_VERSION);
    php_info_print_table_end();

    // DISPLAY_INI_ENTRIES();
}

/* {{{ vapor_module_entry */
zend_module_entry vapor_module_entry = {
    STANDARD_MODULE_HEADER,
    "vapor",
    NULL,
    PHP_MINIT(vapor),
    PHP_MSHUTDOWN(vapor),
    NULL,
    NULL,
    PHP_MINFO(vapor),
    VAPOR_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_VAPOR
    #ifdef ZTS
        ZEND_TSRMLS_CACHE_DEFINE()
    #endif
    ZEND_GET_MODULE(vapor)
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
