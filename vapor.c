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

#include "php_vapor.h"

ZEND_DECLARE_MODULE_GLOBALS(vapor)

zend_object_handlers vapor_object_handlers;
zend_class_entry *vapor_ce;
//static int le_vapor;

/* {{{ PHP_INI */
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("vapor.path", "", PHP_INI_ALL, OnUpdateString, path, zend_vapor_globals, vapor_globals)
    STD_PHP_INI_ENTRY("vapor.extension", "php", PHP_INI_ALL, OnUpdateString, extension, zend_vapor_globals, vapor_globals)
PHP_INI_END()
/* }}} */

/* {{{ vapor_init_globals */
static void vapor_init_globals(zend_vapor_globals *vapor_globals)
{
    memset(vapor_globals, 0, sizeof(zend_vapor_globals));
    vapor_globals->path = "";
    vapor_globals->extension = "php";
}
/* }}} */

/* {{{ vapor_free_storage */
void vapor_free_storage(zend_object *obj)
{
    vapor_tpl *vapor = php_vapor_fetch_object(obj);

    if (vapor->basepath)  efree(vapor->basepath);
    if (vapor->filename)  efree(vapor->filename);
    if (vapor->filepath)  efree(vapor->filepath);
    if (vapor->extension) efree(vapor->extension);
    if (vapor->layout)    efree(vapor->layout);

    if (vapor->folders) {
        zend_hash_destroy(vapor->folders);
        FREE_HASHTABLE(vapor->folders);
    }
    if (vapor->sections) {
        zend_hash_destroy(vapor->sections);
        FREE_HASHTABLE(vapor->sections);
    }
    if (vapor->functions) {
        zend_hash_destroy(vapor->functions);
        FREE_HASHTABLE(vapor->functions);
    }

    zend_object_std_dtor(&vapor->std);
}
/* }}} */

/* {{{ vapor_object_new */
zend_object *vapor_object_new(zend_class_entry *ce)
{
    vapor_tpl *vapor;

    // vapor = ecalloc(1, sizeof(vapor_tpl) + zend_object_properties_size(ce));
    vapor = (vapor_tpl *) ecalloc(1, sizeof(vapor_tpl));

    zend_object_std_init(&vapor->std, ce);
    // object_properties_init(&vapor->std, ce);
    vapor->std.handlers = &vapor_object_handlers;

    return &vapor->std;
}
/* }}} */

/* {{{ vapor_set_property */
static void vapor_set_property(zval *object, zval *key, zval *value, void **cache_slot)
{
    vapor_tpl *vapor = Z_VAPOR_P(object);
    convert_to_string(key);

    std_object_handlers.write_property(object, key, value, cache_slot);
}
/* }}} */

/* {{{ vapor_unset_property */
static void vapor_unset_property(zval *object, zval *key, void **cache_slot)
{
    vapor_tpl *vapor = Z_VAPOR_P(object);
    convert_to_string(key);

    std_object_handlers.unset_property(object, key, cache_slot);
}
/* }}} */

/* {{{ vapor_get_property */
static char *vapor_get_property(zval *object, char *key)
{
    zval *res, rv;

    res = zend_read_property(vapor_ce, object, key, strlen(key), 1, &rv);
    ZVAL_DEREF(res);
    zval_ptr_dtor(&rv);

    if (Z_TYPE_P(res) == IS_STRING && Z_STRLEN_P(res) > 0) {
        zval tmp;
        ZVAL_ZVAL(&tmp, res, 0, 1);
        return Z_STRVAL_P(&tmp);
    }
}
/* }}} */

/* {{{ vapor_copy_userdata */
static void vapor_copy_userdata(HashTable *symtable, HashTable *data)
{
    char *str;
    int i;
    zend_string *key;
    zval *val;

    i = data ? zend_hash_num_elements(data) : 0;
    if (i > 0) {
        ZEND_HASH_FOREACH_STR_KEY_VAL(data, key, val) {
            str = ZSTR_VAL(key);
            Z_TRY_ADDREF_P(val);
            zend_hash_str_update(symtable, str, strlen(str), val);
        }
        ZEND_HASH_FOREACH_END();
    }
}
/* }}} */

/* {{{ vapor_path */
static char *vapor_path(zval *object)
{
    char *filepath;
    vapor_tpl *vapor;

    vapor = Z_VAPOR_P(object);

    if (!vapor->filename) {
        filepath = estrdup(vapor->basepath);
    } else {
        if (strrchr(vapor->filename, '.') == NULL) {
            spprintf(&filepath, 0, "%s/%s.%s", vapor->basepath, vapor->filename, vapor->extension);
        } else {
            spprintf(&filepath, 0, "%s/%s", vapor->basepath, vapor->filename);
        }
    }

    return filepath;
}
/* }}} */

/* {{{ vapor_file_exists */
static int vapor_file_exists(zval *obj, char *filepath)
{
    zend_stat_t fsb;

    if (php_check_open_basedir(filepath)) {
        return 0;
    }

    if (VCWD_STAT(filepath, &fsb) != 0) {
        return 0;
    }

    if (!S_ISREG(fsb.st_mode)) {
        return 0;
    }

    return 1;
}
/* }}} */

/* {{{ zend_bool vapor_get_callback() */
static zend_bool vapor_get_callback(INTERNAL_FUNCTION_PARAMETERS, char *func_name, zval **callback)
{
    vapor_tpl *vapor = Z_VAPOR_P(GetThis());

    if ((*callback = zend_hash_str_find(vapor->functions, func_name, strlen(func_name))) != NULL) {
        if (zend_is_callable(*callback, 0, 0)) {
            return 1;
        }
    }
    return 0;
}
/* }}} */

/* {{{ void vapor_render */
static void vapor_render(char *filepath, HashTable *symtable, HashTable *data, zval *content)
{
    int ob_level;
    zend_file_handle file_handle;
    zend_op_array *op_array;
    zend_stat_t statbuf;
    zval result;

    if (data != NULL && symtable) {
        vapor_copy_userdata(symtable, data);
    }

    if (VCWD_STAT(filepath, &statbuf) != 0 || S_ISREG(statbuf.st_mode) == 0) {
        php_error_docref(NULL, E_WARNING, "Unable to load template file");
        return;
    }

    if (php_check_open_basedir(filepath)) {
        php_error_docref(NULL, E_WARNING, "open_basedir restriction in effect. Unable to open file");
        return;
    }

    file_handle.type          = ZEND_HANDLE_FILENAME;
    file_handle.handle.fp     = NULL;
    file_handle.filename      = filepath;
    file_handle.opened_path   = NULL;
    file_handle.free_filename = 0;

    op_array = zend_compile_file(&file_handle, ZEND_INCLUDE);
    zend_destroy_file_handle(&file_handle);

    php_output_start_default();

    if (op_array) {
        ZVAL_UNDEF(&result);

        zend_try {
            zend_execute(op_array, &result);

            destroy_op_array(op_array);
            efree(op_array);
            zval_ptr_dtor(&result);
        } zend_catch {
            zend_printf("<fault error>");
        } zend_end_try();
    }

    php_output_get_contents(content);
    php_output_discard();
}
/* }}} */

/* {{{ proto void Vapor::__construct(string path, string extension = null) */
static PHP_METHOD(Vapor, __construct)
{
    char *path, *ext = NULL, resolved_path[MAXPATHLEN];
    size_t path_len, ext_len;
    vapor_tpl *vapor;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(path, path_len)
        Z_PARAM_OPTIONAL
        Z_PARAM_STRING(ext, ext_len)
    ZEND_PARSE_PARAMETERS_END();

    vapor = Z_VAPOR_P(GetThis());

    if (!VCWD_REALPATH(path, resolved_path)) {
        zend_throw_exception_ex(zend_ce_exception, 0, "Could not resolve file path");
    }

    vapor->basepath  = estrdup(resolved_path);
    vapor->filename  = NULL;
    vapor->filepath  = NULL;
    vapor->layout    = NULL;
    vapor->extension = (ext) ? estrdup(ext) : NULL;

    if (!vapor->folders) {
        ALLOC_HASHTABLE(vapor->folders);
        zend_hash_init(vapor->folders, VAPOR_MAX_FOLDERS, NULL, ZVAL_PTR_DTOR, 0);
    }
    if (!vapor->sections) {
        ALLOC_HASHTABLE(vapor->sections);
        zend_hash_init(vapor->sections, VAPOR_MAX_SECTIONS, NULL, ZVAL_PTR_DTOR, 0);
    }
    if (!vapor->functions) {
        ALLOC_HASHTABLE(vapor->functions);
        zend_hash_init(vapor->functions, VAPOR_MAX_FUNCTIONS, NULL, ZVAL_PTR_DTOR, 0);
    }
}
/* }}} */

/* {{{ proto string Vapor::__call(...) */
static PHP_METHOD(Vapor, __call)
{
    char *function;
    size_t len;
    int argc;
    zval *callback, *element, *params, *params_array;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(function, len)
        Z_PARAM_ZVAL(params_array)
    ZEND_PARSE_PARAMETERS_END();

    argc   = zend_hash_num_elements(Z_ARRVAL_P(params_array));
    params = safe_emalloc(sizeof(zval), argc, 0);

    argc = 0;
    ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(params_array), element) {
        ZVAL_COPY(&params[argc], element);
        argc++;
    }
    ZEND_HASH_FOREACH_END();

    if (vapor_get_callback(INTERNAL_FUNCTION_PARAM_PASSTHRU, function, &callback)) {
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

    for (int i = 0; i < argc; i ++) {
        zval_ptr_dtor(&params[i]);
    }

    efree(params);
}
/* }}} */

/* {{{ proto void Vapor::addFolder(string path) */
static PHP_METHOD(Vapor, addFolder)
{
    vapor_tpl *vapor;
    char *folder, resolved_path[MAXPATHLEN];
    size_t len;
    zval *val;
    zend_long key;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(folder, len)
    ZEND_PARSE_PARAMETERS_END();

    if (len <= 0) {
        php_error_docref(NULL, E_WARNING, "parameter 1 must not be empty");
        return;
    }
    if (!VCWD_REALPATH(folder, resolved_path)) {                                        \
        zend_throw_exception_ex(zend_ce_exception, 0, "Could not resolve file path"); \
    }

    vapor = Z_VAPOR_P(GetThis());

    // try zend_hash_find?
    int found = 0;
    ZEND_HASH_FOREACH_NUM_KEY_VAL(vapor->folders, key, val) {
        if (strcmp(Z_STRVAL_P(val), resolved_path) == 0) {
            found = 1;
            break;
        }
    }
    ZEND_HASH_FOREACH_END();

    if (found == 0) {
        zval zv_path;
        ZVAL_STRING(&zv_path, resolved_path);
        zend_hash_next_index_insert(vapor->folders, &zv_path);
    }
}
/* }}} */

/* {{{ proto array Vapor::getFolders() */
static PHP_METHOD(Vapor, getFolders)
{
    HashTable *folders = Z_VAPOR_P(getThis())->folders;
    RETURN_ARR(zend_array_dup(folders));
}
/* }}} */

/* {{{ proto void Vapor::setExtension(string extension) */
static PHP_METHOD(Vapor, setExtension)
{
    char *ext;
    size_t ext_len;
    vapor_tpl *vapor;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(ext, ext_len)
    ZEND_PARSE_PARAMETERS_END();

    vapor = Z_VAPOR_P(GetThis());

    // free original value
    if (vapor->extension) {
        efree(vapor->extension);
    }
    vapor->extension = estrdup(ext);
}
/* }}} */

/* {{{ proto void Vapor::register(string name, callable userfunc) */
static PHP_METHOD(Vapor, register)
{
    char *name;
    size_t len;
    vapor_tpl *vapor;
    zval *func, tmp;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(name, len)
        Z_PARAM_ZVAL(func)
    ZEND_PARSE_PARAMETERS_END();

    tmp = *func;
    zval_copy_ctor(&tmp);

    vapor = Z_VAPOR_P(GetThis());
    zend_hash_str_update(vapor->functions, name, len, &tmp);
}
/* }}} */

/* {{{ proto string Vapor::path() */
static PHP_METHOD(Vapor, path)
{
    char *path = NULL;

    if (zend_parse_parameters_none() == FAILURE) {
        return;
    }

    path = vapor_path(GetThis());
    if (path) {
        ZVAL_STRING(return_value, path);
    }
    efree(path);
}
/* }}} */

/* {{{ proto void Vapor::layout(string layout) */
static PHP_METHOD(Vapor, layout)
{
    char *layout;
    size_t len;
    vapor_tpl *vapor;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(layout, len)
    ZEND_PARSE_PARAMETERS_END();

    vapor = Z_VAPOR_P(GetThis());

    if (vapor->layout) {
        efree(vapor->layout);
    }
    vapor->layout = estrdup(layout);
}
/* }}} */

/* {{{ proto string Vapor::section(string section) */
static PHP_METHOD(Vapor, section)
{
    char *content, *section;
    size_t len;
    vapor_tpl *vapor;
    zval *tmp;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(section, len)
    ZEND_PARSE_PARAMETERS_END();

    vapor = Z_VAPOR_P(GetThis());

    if (NULL != (tmp = zend_hash_str_find(vapor->sections, section, len)) && Z_TYPE_P(tmp) == IS_STRING) {
        RETURN_STRING(Z_STRVAL_P(tmp));
    }
}
/* }}} */

/* {{{ proto string Vapor::include(string filename) */
static PHP_METHOD(Vapor, include)
{
    char *filename;
    size_t len;
    vapor_tpl *vapor;
    zval *object;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(filename, len)
    ZEND_PARSE_PARAMETERS_END();

    object = GetThis();
    vapor = Z_VAPOR_P(object);

    if (vapor->filename) efree(vapor->filename);
    vapor->filename = estrdup(filename);

    if (vapor->filepath) efree(vapor->filepath);
    vapor->filepath = vapor_path(object);

    zval content;
    ZVAL_UNDEF(&content);
    vapor_render(vapor->filepath, NULL, NULL, &content);

    RETURN_ZVAL(&content, 0, 1);
}
/* }}} */

/* {{{ proto string Vapor::escape(string var) */
static PHP_METHOD(Vapor, escape)
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
            ZEND_HASH_FOREACH_VAL(Z_ARRVAL(used_callbacks), funcname) {
                zval *callback;
                if (vapor_get_callback(INTERNAL_FUNCTION_PARAM_PASSTHRU, Z_STRVAL_P(funcname), &callback)) {
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
/* }}} */

/* {{{ proto string Vapor::render(string filename, [] data) */
static PHP_METHOD(Vapor, render)
{
    char *filename = NULL;
    HashTable *data = NULL;
    size_t len;
    vapor_tpl *vapor;
    zval content, *object;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(filename, len)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_HT(data)
    ZEND_PARSE_PARAMETERS_END();

    object = GetThis();
    vapor = Z_VAPOR_P(object);

    vapor->filename = estrndup(filename, len);
    vapor->filepath = vapor_path(object);

    ZVAL_UNDEF(&content);   // must initialize it
    vapor_render(vapor->filepath, &EG(symbol_table), data, &content);

    if (vapor->layout != NULL) {
        zval content_copy;
        ZVAL_ZVAL(&content_copy, &content, 0, 1);
        zend_hash_str_update(vapor->sections, "content", sizeof("content")-1, &content_copy);

        vapor_set_value(filename, vapor->layout, 1);
        vapor_set_value(filepath, vapor_path(object), 0);
        vapor_set_null(layout);

        vapor_render(vapor->filepath, &EG(symbol_table), NULL, &content);
    }

    vapor_set_null(filename);
    vapor_set_null(filepath);

    // RETURN_ZVAL(&content, 0, 1);
    ZVAL_ZVAL(return_value, &content, 0, 1);
}
/* }}} */

/* {{{ arginfo */
ZEND_BEGIN_ARG_INFO_EX(arginfo__construct, 0, 0, 1)
    ZEND_ARG_INFO(0, path)
    ZEND_ARG_INFO(0, extension)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo__call, 0, 0, 2)
    ZEND_ARG_INFO(0, function_name)
    ZEND_ARG_INFO(0, arguments)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_add_folder, 0, 0, 1)
    ZEND_ARG_INFO(0, folder)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_set_extension, 0, 0, 1)
    ZEND_ARG_INFO(0, extension)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_register, 0, 0, 2)
    ZEND_ARG_INFO(0, name)
    ZEND_ARG_INFO(0, userfunc)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_layout, 0, 0, 1)
    ZEND_ARG_INFO(0, layout)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_section, 0, 0, 1)
    ZEND_ARG_INFO(0, section)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_include, 0, 0, 1)
    ZEND_ARG_INFO(0, filename)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_escape, 0, 0, 1)
    ZEND_ARG_INFO(0, var)
    ZEND_ARG_INFO(0, funcs)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_render, 0, 0, 1)
    ZEND_ARG_INFO(0, filename)
    ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ vapor_methods[] : Vapor class */
static const zend_function_entry vapor_methods[] = {
    PHP_ME(Vapor,   __construct,    arginfo__construct,     ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(Vapor,   __call,         arginfo__call,          ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,   addFolder,      arginfo_add_folder,     ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,   getFolders,     NULL,                   ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,   setExtension,   arginfo_set_extension,  ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,   register,       arginfo_register,       ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,   path,           NULL,                   ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,   layout,         arginfo_layout,         ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,   section,        arginfo_section,        ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,   include,        arginfo_include,        ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,   escape,         arginfo_escape,         ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,   render,         arginfo_render,         ZEND_ACC_PUBLIC)
    PHP_MALIAS(Vapor,   e,      escape,     arginfo_escape,     ZEND_ACC_PUBLIC)
    PHP_MALIAS(Vapor,   insert, include,    arginfo_include,    ZEND_ACC_PUBLIC)
    PHP_FE_END
};
/* }}} */

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(vapor)
{
    zend_class_entry ce;

    // register resource dtor
    // le_vapor = zend_register_list_destructors_ex(vapor_resource_dtor, NULL, "Vapor Template", module_number);

    REGISTER_INI_ENTRIES();

    INIT_CLASS_ENTRY(ce, "Vapor", vapor_methods);
    ce.create_object = vapor_object_new;
    vapor_ce = zend_register_internal_class(&ce);

    memcpy(&vapor_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    vapor_object_handlers.offset   = XtOffsetOf(vapor_tpl, std);
    vapor_object_handlers.free_obj = vapor_free_storage;
    // vapor_object_handlers.read_property  = vapor_get_property;
    // vapor_object_handlers.write_property = vapor_set_property;

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(vapor)
{
    UNREGISTER_INI_ENTRIES();
    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(vapor)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "Vapor Support", "enabled");
    php_info_print_table_row(2, "Version", VAPOR_VERSION);
    php_info_print_table_end();

    DISPLAY_INI_ENTRIES();
}
/* }}} */

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
/* }}} */

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
