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

#include "debug.h"
#include "vapor.h"

zend_object_handlers vapor_object_handlers;
zend_class_entry    *vapor_ce;
//static int le_vapor;

// ZEND_DECLARE_MODULE_GLOBALS(vapor)

/* {{{ vapor_init_globals */
// static void vapor_init_globals(zend_vapor_globals *vapor_globals)
// {
//     memset(vapor_globals, 0, sizeof(zend_vapor_globals));
//     vapor_globals->path = "";
//     vapor_globals->extension = "php";
// }
/* }}} */

/* {{{ PHP_INI */
// PHP_INI_BEGIN()
//     STD_PHP_INI_ENTRY("vapor.path", "", PHP_INI_ALL, OnUpdateString, path, zend_vapor_globals, vapor_globals)
//     STD_PHP_INI_ENTRY("vapor.extension", "php", PHP_INI_ALL, OnUpdateString, extension, zend_vapor_globals, vapor_globals)
// PHP_INI_END()
/* }}} */

/* {{{ void vapor_free_storage(zend_object *obj) */
void vapor_free_storage(zend_object *obj)
{
    vapor_core *vapor = php_vapor_fetch_object(obj);

    if (vapor->basepath)  efree(vapor->basepath);
    if (vapor->extension) efree(vapor->extension);

    if (vapor->folders) {
        zend_hash_destroy(vapor->folders);
        FREE_HASHTABLE(vapor->folders);
    }
    if (vapor->functions) {
        zend_hash_destroy(vapor->functions);
        FREE_HASHTABLE(vapor->functions);
    }
    if (vapor->sections) {
        zend_hash_destroy(vapor->sections);
        FREE_HASHTABLE(vapor->sections);
    }

    zend_object_std_dtor(&vapor->std);
}
/* }}} */

/* {{{ zend_object *vapor_object_new(zend_class_entry *ce) */
zend_object *vapor_object_new(zend_class_entry *ce)
{
    vapor_core *vapor;

    // vapor = ecalloc(1, sizeof(vapor_core) + zend_object_properties_size(ce));
    vapor = (vapor_core *)ecalloc(1, sizeof(vapor_core));

    zend_object_std_init(&vapor->std, ce);
    // object_properties_init(&vapor->std, ce);
    vapor->std.handlers = &vapor_object_handlers;

    return &vapor->std;
}
/* }}} */

/* {{{ void vapor_set_property(zval *object, zval *key, zval *value, void **cache_slot) */
static void vapor_set_property(zval *object, zval *key, zval *value, void **cache_slot)
{
    vapor_core *vapor = Z_VAPOR_P(object);
    convert_to_string(key);

    std_object_handlers.write_property(object, key, value, cache_slot);
}
/* }}} */

/* {{{ void vapor_unset_property(zval *object, zval *key, void **cache_slot) */
static void vapor_unset_property(zval *object, zval *key, void **cache_slot)
{
    vapor_core *vapor = Z_VAPOR_P(object);
    convert_to_string(key);

    std_object_handlers.unset_property(object, key, cache_slot);
}
/* }}} */

/* {{{ char *vapor_get_property(zval *object, char *key) */
static char *vapor_get_property(zval *object, char *key)
{
    zval *res, rv;

    res = zend_read_property(vapor_ce, object, key, strlen(key), 1, &rv);
    ZVAL_DEREF(res);
    zval_ptr_dtor(&rv);

    if (Z_TYPE_P(res) == IS_STRING && Z_STRLEN_P(res) > 0) {
        zval tmp;
        ZVAL_ZVAL(&tmp, res, 0, 1);
        return Z_STRVAL(tmp);
    }
}
/* }}} */

/* {{{ int vapor_get_callback(INTERNAL_FUNCTION_PARAMETERS, char *func_name, zval **callback) */
static int vapor_get_callback(INTERNAL_FUNCTION_PARAMETERS, char *func_name, zval **callback)
{
    vapor_core *vapor = Z_VAPOR_P(GetThis());

    if ((*callback = zend_hash_str_find(vapor->functions, func_name, strlen(func_name))) != NULL) {
        if (zend_is_callable(*callback, 0, 0)) {
            return 1;
        }
    }
    return 0;
}
/* }}} */

/* {{{ void vapor_copy_userdata(zend_array *symtable, zend_array *data) */
static void vapor_copy_userdata(zend_array *symtable, zend_array *data)
{
    zend_string *key;
    zval *val;

    if (UNEXPECTED(data == NULL)) {
        return;
    }

    ZEND_HASH_FOREACH_STR_KEY_VAL(data, key, val) {
        Z_TRY_ADDREF_P(val);
        zend_hash_str_update(symtable, ZSTR_VAL(key), ZSTR_LEN(key), val);
    }
    ZEND_HASH_FOREACH_END();
}
/* }}} */

/* {{{ void vapor_split_filename(char *filename, char **folder, char **basename) */
static inline void vapor_split_filename(char *filename, char **folder, char **basename)
{
    char *src, *last = NULL;

    src = estrdup(filename);
    if (!strchr(src, ':')) {
        *folder = NULL;
        *basename = estrdup(src);
        efree(src);
        return;
    }

    *folder = estrdup(php_strtok_r(src, ":", &last));
    *basename = estrdup(php_strtok_r(NULL, ":", &last));

    efree(src);
}
/* }}} */

/* {{{ int vapor_check_folder(zend_array *folders, const char *folder) */
static inline int vapor_check_folder(zend_array *folders, const char *folder)
{
    return !folder || (folder && zend_hash_str_exists(folders, folder, sizeof(folder) - 1));
}
/* }}} */

/* {{{ int vapor_filepath(vapor_core *vapor, char *folder, char *basename, char **filepath) */
static inline int vapor_filepath(vapor_core *vapor, char *folder, char *basename, char **filepath)
{
    char buf[MAXPATHLEN];
    zend_string *key;
    zval *val;

    if (folder != NULL) {
        zval *path;
        if ((path = zend_hash_str_find(vapor->folders, folder, sizeof(folder) - 1))) {
            slprintf(buf, sizeof(buf), "%s/%s.%s", Z_STRVAL_P(path), basename, vapor->extension);
            *filepath = estrdup(buf);
            return 1;
        }
    } else {
        slprintf(buf, sizeof(buf), "%s/%s.%s", vapor->basepath, basename, vapor->extension);
        *filepath = estrdup(buf);
        return 1;
    }

    return 0;
}
/* }}} */

/* {{{ vapor_template *vapor_new_template(vapor_core *vapor, char *folder, char *basename, char *filepath, int attach) */
static vapor_template *vapor_new_template(vapor_core *vapor, char *folder, char *basename, char *filepath, int attach)
{
    vapor_template *tpl;

    tpl = emalloc(sizeof(vapor_template));
    memset(tpl, 0, sizeof(vapor_template));

    tpl->folder   = folder;
    tpl->basename = basename;
    tpl->filepath = filepath;

    if (attach) vapor->current = tpl;
    return tpl;
}
/* }}} */

/* {{{ void vapor_execute(vapor_template *tpl, zval *content) */
static void vapor_execute(vapor_template *tpl, zval *content)
{
    zend_file_handle file_handle;
    zend_op_array *op_array;
    zend_stat_t statbuf;
    zval retval;

    if (VCWD_STAT(tpl->filepath, &statbuf) != 0 || S_ISREG(statbuf.st_mode) == 0) {
        zend_throw_exception_ex(zend_ce_exception, 0, "Unable to load template file - %s", tpl->filepath);
        zend_bailout();
    }

    if (php_check_open_basedir(tpl->filepath)) {
        zend_throw_exception_ex(zend_ce_exception, 0, "open_basedir restriction in effect. Unable to open file.");
        zend_bailout();
    }

    file_handle.type          = ZEND_HANDLE_FILENAME;
    file_handle.handle.fp     = NULL;
    file_handle.filename      = tpl->filepath;
    file_handle.opened_path   = NULL;
    file_handle.free_filename = 0;

    php_output_start_default();

    op_array = zend_compile_file(&file_handle, ZEND_INCLUDE);
    zend_destroy_file_handle(&file_handle);

    if (EXPECTED(op_array != NULL)) {
        ZVAL_UNDEF(&retval);

        zend_execute(op_array, &retval);
        destroy_op_array(op_array);
        efree(op_array);
    }

    php_output_get_contents(content);
    php_output_discard();
}
/* }}} */

/* {{{ void vapor_run(vapor_core *vapor, vapor_template *tpl, zval *content) */
static void vapor_run(vapor_core *vapor, vapor_template *tpl, zval *content)
{
    char *folder = NULL, *basename = NULL, *filepath = NULL;

    // 执行 PHP 文件
    vapor_execute(tpl, content);

    // 如果模板文件指定了 layout，递归调用本函数，执行 layout 文件
    if (tpl->layout) {
        zval tmp;
        ZVAL_ZVAL(&tmp, content, 1, 1);
        zend_hash_str_update(vapor->sections, "content", sizeof("content") - 1, &tmp);

        vapor_run(vapor, tpl->layout, content);
    }

    efree(tpl->folder);
    efree(tpl->basename);
    efree(tpl->filepath);
    efree(tpl);
}
/* }}} */

/* {{{ proto void Vapor::__construct(string path, string extension = null) */
static PHP_METHOD(Vapor, __construct)
{
    char *path, *ext = NULL, resolved_path[MAXPATHLEN];
    size_t path_len, ext_len;
    vapor_core *vapor;

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
    vapor->extension = (ext) ? estrdup(ext): NULL;

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

    for (int i = 0; i < argc; i++) {
        zval_ptr_dtor(&params[i]);
    }

    efree(params);
}
/* }}} */

/* {{{ proto void Vapor::addFolder(string name, string path, bool fallback) */
static PHP_METHOD(Vapor, addFolder)
{
    char *name, *folder;
    char resolved_path[MAXPATHLEN];
    size_t len1, len2;
    vapor_core *vapor;
    zend_bool fallback = 0;
    zend_string *key;
    zval *val;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(name, len1)
        Z_PARAM_STRING(folder, len2)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL(fallback)
    ZEND_PARSE_PARAMETERS_END();

    if (!VCWD_REALPATH(folder, resolved_path)) {
        zend_throw_exception_ex(zend_ce_exception, 0, "Could not resolve file path");
    }

    vapor = Z_VAPOR_P(GetThis());

    zval zv;
    ZVAL_STRING(&zv, resolved_path);
    zend_hash_str_update(vapor->folders, name, sizeof(name)-1, &zv);
}
/* }}} */

/* {{{ proto array Vapor::getFolders() */
static PHP_METHOD(Vapor, getFolders)
{
    vapor_core *vapor = Z_VAPOR_P(GetThis());
    RETURN_ARR(zend_array_dup(vapor->folders));
}
/* }}} */

/* {{{ proto void Vapor::setExtension(string extension) */
static PHP_METHOD(Vapor, setExtension)
{
    char *ext;
    size_t ext_len;
    vapor_core *vapor;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(ext, ext_len)
    ZEND_PARSE_PARAMETERS_END();

    vapor = Z_VAPOR_P(GetThis());
    if (vapor->extension) efree(vapor->extension);
    vapor->extension = estrdup(ext);
}
/* }}} */

/* {{{ proto void Vapor::registerFunction(string name, callable userfunc) */
static PHP_METHOD(Vapor, registerFunction)
{
    char *name;
    size_t len;
    vapor_core *vapor;
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
    char *filename = NULL;
    size_t len;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_STRING(filename, len)
    ZEND_PARSE_PARAMETERS_END();

    vapor_core *vapor = Z_VAPOR_P(GetThis());
    php_printf(vapor->current->filepath);
}
/* }}} */

/* {{{ proto void Vapor::layout(string layout) */
static PHP_METHOD(Vapor, layout)
{
    char *layout, *folder = NULL, *basename = NULL, *filepath= NULL;
    size_t len;
    vapor_core *vapor;
    zend_array *data = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(layout, len)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_HT(data)
    ZEND_PARSE_PARAMETERS_END();

    vapor = Z_VAPOR_P(GetThis());

    if (UNEXPECTED(vapor->current == NULL)) {
        zend_throw_exception_ex(zend_ce_exception, 0, "Failed to set layout");
        zend_bailout();
    }

    vapor_split_filename(layout, &folder, &basename);

    if (!vapor_check_folder(vapor->folders, folder)) {
        zend_throw_exception_ex(zend_ce_exception, 0, "Folder %s does not exists", folder);
        zend_bailout();
    }

    if (!vapor_filepath(vapor, folder, basename, &filepath)) {
        zend_throw_exception_ex(zend_ce_exception, 0, "Can't get filepath");
        zend_bailout();
    }

    vapor->current->layout = vapor_new_template(vapor, folder, basename, filepath, 1);

    if (UNEXPECTED(data != NULL)) {
        vapor_copy_userdata(zend_rebuild_symbol_table(), data);
    }
}
/* }}} */

/* {{{ proto string Vapor::section(string section) */
static PHP_METHOD(Vapor, section)
{
    char *content, *section;
    size_t len;
    vapor_core *vapor;
    zval *tmp;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(section, len)
    ZEND_PARSE_PARAMETERS_END();

    vapor = Z_VAPOR_P(GetThis());

    if (NULL != (tmp = zend_hash_str_find(vapor->sections, section, len)) && Z_TYPE_P(tmp) == IS_STRING) {
        RETURN_STRING(Z_STRVAL_P(tmp));
    }
    RETURN_NULL();
}
/* }}} */

/* {{{ proto void Vapor::insert(string filename, [] data) */
static PHP_METHOD(Vapor, insert)
{
    char *filename, *folder = NULL, *basename = NULL, *filepath = NULL;
    size_t len;
    vapor_core *vapor;
    vapor_template *tpl;
    zend_array *data = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(filename, len)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_HT(data)
    ZEND_PARSE_PARAMETERS_END();

    vapor = Z_VAPOR_P(GetThis());

    vapor_split_filename(filename, &folder, &basename);

    if (!vapor_check_folder(vapor->folders, folder)) {
        zend_throw_exception_ex(zend_ce_exception, 0, "Folder %s does not exists", folder);
        zend_bailout();
    }

    if (!vapor_filepath(vapor, folder, basename, &filepath)) {
        zend_throw_exception_ex(zend_ce_exception, 0, "Can't get filepath");
        zend_bailout();
    }

    if (UNEXPECTED(data != NULL)) {
        vapor_copy_userdata(zend_rebuild_symbol_table(), data);
    }

    tpl = vapor_new_template(vapor, folder, basename, filepath, 0);

    zval content;
    ZVAL_UNDEF(&content);
    vapor_execute(tpl, &content);

    efree(tpl->folder);
    efree(tpl->basename);
    efree(tpl->filepath);
    efree(tpl);

    php_printf(Z_STRVAL_P(&content));
    zval_ptr_dtor(&content);
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
            ZEND_HASH_FOREACH_VAL (Z_ARRVAL(used_callbacks), funcname) {
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

/* {{{ proto string Vapor::render(string tplname, array data) */
static PHP_METHOD(Vapor, render)
{
    char *tplname = NULL, *folder = NULL, *basename = NULL, *filepath = NULL;
    size_t len;
    vapor_core *vapor;
    vapor_template *tpl;
    zend_array *data = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(tplname, len)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_HT(data)
    ZEND_PARSE_PARAMETERS_END();

    vapor = Z_VAPOR_P(GetThis());

    vapor_split_filename(tplname, &folder, &basename);

    if (!vapor_check_folder(vapor->folders, folder)) {
        zend_throw_exception_ex(zend_ce_exception, 0, "Folder %s does not exists", folder);
        zend_bailout();
    }

    if (!vapor_filepath(vapor, folder, basename, &filepath)) {
        zend_throw_exception_ex(zend_ce_exception, 0, "Can't get filepath");
        zend_bailout();
    }

    if (EXPECTED(data != NULL)) {
        vapor_copy_userdata(zend_rebuild_symbol_table(), data);
    }

    tpl = vapor_new_template(vapor, folder, basename, filepath, 1);

    zval content;
    ZVAL_UNDEF(&content);
    vapor_run(vapor, tpl, &content);

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

ZEND_BEGIN_ARG_INFO_EX(arginfo_render, 0, 0, 1)
    ZEND_ARG_INFO(0, filename)
    ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()
/* }}} */

/* {{{ vapor_methods[] : Vapor class */
static const zend_function_entry vapor_methods[] = {
    PHP_ME(Vapor,       __construct,        arginfo__construct,     ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(Vapor,       __call,             arginfo__call,          ZEND_ACC_PUBLIC | ZEND_ACC_FINAL)
    PHP_ME(Vapor,       addFolder,          arginfo_add_folder,     ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,       getFolders,         NULL,                   ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,       setExtension,       arginfo_set_extension,  ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,       registerFunction,   arginfo_register_func,  ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,       path,               NULL,                   ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,       layout,             arginfo_layout,         ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,       section,            arginfo_section,        ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,       insert,             arginfo_insert,         ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,       escape,             arginfo_escape,         ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,       render,             arginfo_render,         ZEND_ACC_PUBLIC)
    PHP_MALIAS(Vapor,   e,      escape,     arginfo_escape,         ZEND_ACC_PUBLIC)
    PHP_FE_END
};
/* }}} */

/* {{{ PHP_MINIT_FUNCTION */
PHP_MINIT_FUNCTION(vapor)
{
    zend_class_entry ce;

    // register resource dtor
    // le_vapor = zend_register_list_destructors_ex(vapor_resource_dtor, NULL, "Vapor Template", module_number);

    // REGISTER_INI_ENTRIES();

    INIT_CLASS_ENTRY(ce, "Vapor", vapor_methods);
    ce.create_object = vapor_object_new;
    vapor_ce         = zend_register_internal_class(&ce);

    memcpy(&vapor_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    vapor_object_handlers.offset   = XtOffsetOf(vapor_core, std);
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

    // DISPLAY_INI_ENTRIES();
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
