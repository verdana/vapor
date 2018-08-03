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
zend_class_entry *vapor_ce;
zend_class_entry *vapor_ce_exception;
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

/* {{{ void vapor_report_error(vapor_core *obj, char *format, ...) */
static void vapor_report_error(vapor_core *obj, char *format, ...)
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
/* }}} */

/* {{{ void vapor_free_storage(zend_object *obj) */
static void vapor_free_storage(zend_object *obj)
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

/* {{{ int vapor_prepare_template(vapor_core *vapor, vapor_template *tpl, char *tplname) */
static int vapor_prepare_template(vapor_core *vapor, vapor_template *tpl, char *tplname)
{
    char *folder = NULL, *basename = NULL, *remain = NULL;
    char path_buf[MAXPATHLEN];

    if (!strrchr(tplname, ':')) {
        folder   = NULL;
        basename = tplname;
    } else {
        folder   = php_strtok_r(tplname, ":", &remain);
        basename = php_strtok_r(NULL, ":", &remain);
    }

    // Check if folder exists and build filepath
    if (folder) {
        if (!zend_hash_str_exists(vapor->folders, folder, strlen(folder))) {
            vapor_report_error(vapor, "Folder \"%s\" does not exists", folder);
            efree(tplname);
            return FAILURE;
        }
        zval *path = zend_hash_str_find(vapor->folders, folder, strlen(folder));
        if (vapor->extension) {
            slprintf(path_buf, sizeof(path_buf), "%s/%s.%s", Z_STRVAL_P(path), basename, vapor->extension);
        } else {
            slprintf(path_buf, sizeof(path_buf), "%s/%s", Z_STRVAL_P(path), basename);
        }
    } else {
        if (vapor->extension) {
            slprintf(path_buf, sizeof(path_buf), "%s/%s.%s", vapor->basepath, basename, vapor->extension);
        } else {
            slprintf(path_buf, sizeof(path_buf), "%s/%s", vapor->basepath, basename);
        }
    }

    // All members must be initialized
    tpl->ready    = 1;
    tpl->folder   = (folder != NULL) ? estrdup(folder) : NULL;
    tpl->basename = estrdup(basename);
    tpl->filepath = estrdup(path_buf);
    tpl->layout   = NULL;

    efree(tplname);
    return SUCCESS;
}
/* }}} */

/* {{{ void vapor_free_template(vapor_template *tpl) */
static void vapor_free_template(vapor_template *tpl)
{
    if (!tpl) return;

    if (tpl->folder) efree(tpl->folder);
    if (tpl->basename) efree(tpl->basename);
    if (tpl->filepath) efree(tpl->filepath);

    efree(tpl);
}
/* }}} */

/* {{{ zend_object *vapor_new_object(zend_class_entry *ce) */
static inline zend_object *vapor_new_object(zend_class_entry *ce)
{
    vapor_core *vapor;

    vapor = ecalloc(1, sizeof(vapor_core) + zend_object_properties_size(ce));

    zend_object_std_init(&vapor->std, ce);
    object_properties_init(&vapor->std, ce);
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

/* {{{ zval *vapor_get_property(zval *object, zval *member, int type, void **cache_slot, zval *rv) */
static zval *vapor_get_property(zval *object, zval *member, int type, void **cache_slot, zval *rv)
{
    vapor_core *vapor;
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
/* }}} */

/* {{{ void vapor_unset_property(zval *object, zval *key, void **cache_slot) */
static void vapor_unset_property(zval *object, zval *key, void **cache_slot)
{
    vapor_core *vapor = Z_VAPOR_P(object);
    convert_to_string(key);

    std_object_handlers.unset_property(object, key, cache_slot);
}
/* }}} */

/* {{{ int vapor_get_callback(INTERNAL_FUNCTION_PARAMETERS, char *func_name, zval **callback) */
static int vapor_get_callback(INTERNAL_FUNCTION_PARAMETERS, char *func_name, zval **callback)
{
    vapor_core *vapor = Z_VAPOR_P(GetThis());

    if ((*callback = zend_hash_str_find(vapor->functions, func_name, strlen(func_name))) != NULL) {
        if (zend_is_callable(*callback, 0, 0)) {
            return SUCCESS;
        }
    }
    return FAILURE;
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
    } ZEND_HASH_FOREACH_END();
}
/* }}} */

/* {{{ void vapor_execute(vapor_template *tpl, zval *content) */
static void vapor_execute(vapor_template *tpl, zval *content)
{
    zend_file_handle file_handle;
    zend_op_array *op_array;
    zval retval;

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
    zend_stat_t statbuf;

    // 检查模板结构体是否准备就绪
    // tpl 的准备工作是有可能失败的
    // 比如用户指定的 folder 或 filename 无效
    if (tpl->ready != 1) {
        vapor_free_template(tpl);
        return;
    }

    // 检查模板文件路径是否正确
    if (VCWD_STAT(tpl->filepath, &statbuf) != 0 || S_ISREG(statbuf.st_mode) == 0) {
        vapor_report_error(vapor, "Unable to load template file");
        vapor_free_template(tpl);
        return;
    }

    // 检查模板文件是否被 open_basedir 限制
    if (php_check_open_basedir(tpl->filepath)) {
        vapor_report_error(vapor, "open_basedir restriction in effect, unable to open file");
        vapor_free_template(tpl);
        return;
    }

    // 清理 content 已有的数据
    // 是由 php_output_get_contents 赋予的在堆上的字符串
    // 所以再次使用 content 的时候，必须保证之前的内存已经被释放
    if (content) {
        zval_ptr_dtor(content);
        ZVAL_UNDEF(content);
    }

    // 执行 PHP 文件
    vapor_execute(tpl, content);

    // 如果模板文件指定了 layout，递归调用本函数，执行 layout 文件
    if (tpl->layout) {
        zval tmp;
        ZVAL_ZVAL(&tmp, content, 1, 0); // just COPY
        zend_hash_str_update(vapor->sections, "content", sizeof("content") - 1, &tmp);

        vapor->current = tpl->layout;
        vapor_run(vapor, tpl->layout, content);
    }

    vapor_free_template(tpl);
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
        vapor_report_error(vapor, "Could not resolve file path");
        return;
    }

    vapor->basepath  = estrdup(resolved_path);
    vapor->extension = (ext) ? estrdup(ext): NULL;

    ALLOC_HASHTABLE(vapor->folders);
    ALLOC_HASHTABLE(vapor->sections);
    ALLOC_HASHTABLE(vapor->functions);

    zend_hash_init(vapor->folders, 0, NULL, ZVAL_PTR_DTOR, 0);
    zend_hash_init(vapor->sections, 0, NULL, ZVAL_PTR_DTOR, 0);
    zend_hash_init(vapor->functions, 0, NULL, ZVAL_PTR_DTOR, 0);

    zend_update_property_string(vapor_ce, GetThis(), "basepath", sizeof("basepath") - 1, vapor->basepath);
    if (vapor->extension) {
        zend_update_property_string(vapor_ce, GetThis(), "extension", sizeof("extension") - 1, vapor->extension);
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
/* }}} */

/* {{{ proto void Vapor::addFolder(string name, string path, bool fallback) */
static PHP_METHOD(Vapor, addFolder)
{
    char *folder, *path;
    char resolved_path[MAXPATHLEN];
    size_t folder_len, path_len;
    vapor_core *vapor;
    zend_bool fallback = 0;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STRING(folder, folder_len)
        Z_PARAM_STRING(path, path_len)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL(fallback)
    ZEND_PARSE_PARAMETERS_END();

    vapor = Z_VAPOR_P(GetThis());

    if (!path_len || !VCWD_REALPATH(path, resolved_path)) {
        vapor_report_error(vapor, "Could not resolve folder path");
        return;
    }

    zval zv;
    ZVAL_STRING(&zv, resolved_path);
    zend_hash_str_update(vapor->folders, folder, folder_len, &zv);
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

    zend_update_property_string(vapor_ce, GetThis(), ZEND_STRL("extension"), vapor->extension);
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
    char *layout, *layout_copy;
    size_t len;
    vapor_core *vapor;
    vapor_template *tpl;
    zend_array *data = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(layout, len)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_HT(data)
    ZEND_PARSE_PARAMETERS_END();

    vapor = Z_VAPOR_P(GetThis());

    if (UNEXPECTED(vapor->current == NULL)) {
        vapor_report_error(vapor, "Failed to set layout");
        return;
    }

    // freed in vapor_run()
    tpl = emalloc(sizeof(vapor_template));
    memset(tpl, 0, sizeof(vapor_template));
    vapor->current->layout = tpl;

    // freed in vapor_prepare_template()
    layout_copy = estrdup(layout);
    if (SUCCESS == vapor_prepare_template(vapor, tpl, layout_copy)) {
        // copy data into symbol table
        if (UNEXPECTED(data != NULL)) {
            vapor_copy_userdata(zend_rebuild_symbol_table(), data);
        }
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
    char *filename, *filename_copy;
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

    // freed in vapor_run()
    tpl = emalloc(sizeof(vapor_template));
    memset(tpl, 0, sizeof(vapor_template));

    vapor = Z_VAPOR_P(GetThis());
    vapor->current = tpl;

    // freed in vapor_prepare_template()
    filename_copy = estrdup(filename);
    if (SUCCESS == vapor_prepare_template(vapor, tpl, filename_copy)) {
        if (UNEXPECTED(data != NULL)) {
            vapor_copy_userdata(zend_rebuild_symbol_table(), data);
        }

        // Run script
        zval content;
        ZVAL_UNDEF(&content);
        vapor_execute(tpl, &content);

        // Free template
        vapor_free_template(tpl);

        // Write output
        php_output_write(Z_STRVAL(content), Z_STRLEN(content));
        zval_ptr_dtor(&content);
        return;
    }

    vapor_free_template(tpl);
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
/* }}} */

/* {{{ proto string Vapor::render(string tplname, array data) */
static PHP_METHOD(Vapor, render)
{
    char *tplname, *tplname_copy;
    size_t len;
    vapor_core *vapor;
    vapor_template *tpl;
    zend_array *data = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(tplname, len)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_HT(data)
    ZEND_PARSE_PARAMETERS_END();

    // freed in vapor_run()
    tpl   = emalloc(sizeof(vapor_template));
    memset(tpl, 0, sizeof(vapor_template));

    vapor = Z_VAPOR_P(GetThis());
    vapor->current = tpl;

    // freed in vapor_prepare_template()
    tplname_copy = estrdup(tplname);
    if (SUCCESS == vapor_prepare_template(vapor, tpl, tplname_copy)) {
        if (EXPECTED(data != NULL)) {
            vapor_copy_userdata(zend_rebuild_symbol_table(), data);
        }

        zval content;
        ZVAL_UNDEF(&content);
        vapor_run(vapor, tpl, &content);

        ZVAL_ZVAL(return_value, &content, 0, 1);
        return;
    }

    vapor_free_template(tpl);
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

/* {{{ vapor_methods[] : VaporException class */
static const zend_function_entry vapor_exception_methods[] = {
    PHP_FE_END
};
/* }}} */

/* {{{ vapor_methods[] : Vapor class */
static const zend_function_entry vapor_methods[] = {
    PHP_ME(Vapor,       __construct,        arginfo__construct,     ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(Vapor,       __call,             arginfo__call,          ZEND_ACC_PUBLIC | ZEND_ACC_FINAL)
    PHP_ME(Vapor,       addFolder,          arginfo_add_folder,     ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,       getFolders,         NULL,                   ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,       setExtension,       arginfo_set_extension,  ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,       registerFunction,   arginfo_register_func,  ZEND_ACC_PUBLIC)
    PHP_ME(Vapor,       path,               arginfo_path,           ZEND_ACC_PUBLIC)
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

    INIT_CLASS_ENTRY(ce, "VaporException", vapor_exception_methods);
    vapor_ce_exception = zend_register_internal_class_ex(&ce, zend_ce_exception);

    INIT_CLASS_ENTRY(ce, "Vapor", vapor_methods);
    ce.create_object = vapor_new_object;
    vapor_ce         = zend_register_internal_class(&ce);

    zend_declare_property_null(vapor_ce, ZEND_STRL("basepath"), ZEND_ACC_PUBLIC);
    zend_declare_property_null(vapor_ce, ZEND_STRL("extension"), ZEND_ACC_PUBLIC);

    memcpy(&vapor_object_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    vapor_object_handlers.offset         = XtOffsetOf(vapor_core, std);
    vapor_object_handlers.free_obj       = vapor_free_storage;
    vapor_object_handlers.read_property  = vapor_get_property;
    vapor_object_handlers.write_property = vapor_set_property;

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION */
PHP_MSHUTDOWN_FUNCTION(vapor)
{
    // UNREGISTER_INI_ENTRIES();
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
