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
#include "ext/standard/php_var.h"
#include "ext/standard/html.h"
#include "ext/standard/html_tables.h"
#include "ext/standard/php_string.h"
#include "zend_interfaces.h"

#define VAPOR_TEMPLATE_GET_OBJ vapor_template *template = Z_VAPOR_TEMPLATE_P(getThis());

zend_object *vapor_template_new(zend_class_entry *ce)
{
    vapor_template *template;

    template = ecalloc(1, sizeof(vapor_template) + zend_object_properties_size(ce));
    zend_object_std_init(&template->std, ce);
    object_properties_init(&template->std, ce);
    template->std.handlers = &vapor_object_handlers_template;

    return &template->std;
}

void vapor_template_free_storage(zend_object *obj)
{
    vapor_template *template = php_vapor_template_from_obj(obj);

    if (template->folder) {
        efree(template->folder);
    }
    if (template->basename) {
        efree(template->basename);
    }
    if (template->filepath) {
        efree(template->filepath);
    }
    if (template->layout_name) {
        efree(template->layout_name);
    }
    if (template->sections) {
        zend_hash_destroy(template->sections);
        FREE_HASHTABLE(template->sections);
    }

    zend_object_std_dtor(&template->std);
}

zval *vapor_object_get_property_template(zval *object, zval *member, int type, void **cache_slot, zval *rv)
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

void vapor_object_set_property_template(zval *object, zval *key, zval *value, void **cache_slot)
{
}

inline void vapor_template_parse_name(char *tplname, char **folder, char **basename)
{
    char *remain;

    if (!strrchr(tplname, ':')) {
        *folder   = NULL;
        *basename = estrdup(tplname);
    } else {
        *folder   = estrdup(php_strtok_r(tplname, ":", &remain));
        *basename = estrdup(php_strtok_r(NULL, ":", &remain));
    }
}

inline int vapor_template_check_folder(zend_array *folders, char *folder)
{
    return zend_hash_str_exists(folders, folder, strlen(folder));
}

void vapor_template_filepath(vapor_engine *engine, char *folder, char *basename, char **filepath)
{
    char path_buf[MAXPATHLEN];

    if (folder) {
        if (!zend_hash_str_exists(engine->folders, folder, strlen(folder))) {
            vapor_report_error(engine, "Folder \"%s\" does not exists", folder);
            return;
        }
        zval *path = zend_hash_str_find(engine->folders, folder, strlen(folder));
        if (engine->extension) {
            slprintf(path_buf, sizeof(path_buf), "%s/%s.%s", Z_STRVAL_P(path), basename, engine->extension);
        } else {
            slprintf(path_buf, sizeof(path_buf), "%s/%s", Z_STRVAL_P(path), basename);
        }
    } else {
        if (engine->extension) {
            slprintf(path_buf, sizeof(path_buf), "%s/%s.%s", engine->basepath, basename, engine->extension);
        } else {
            slprintf(path_buf, sizeof(path_buf), "%s/%s", engine->basepath, basename);
        }
    }

    *filepath = estrdup(path_buf);
}

zval *vapor_template_instantiate(zval *object)
{
	if (UNEXPECTED(object_init_ex(object, vapor_ce_template) != SUCCESS)) {
		return NULL;
	}
    return object;
}

int vapor_template_initialize(vapor_template *tpl, vapor_engine *engine, char *tplname)
{
    char *folder = NULL, *basename = NULL, *filepath = NULL;
    zend_stat_t statbuf;

    // Extract foler and basename
    vapor_template_parse_name(tplname, &folder, &basename);

    // Check folder exists
    if (folder != NULL && !vapor_template_check_folder(engine->folders, folder)) {
        vapor_report_error(engine, "Folder \"%s\" does not exists", folder);
        efree(basename);
        return 0;
    }

    // Update filepath
    vapor_template_filepath(engine, folder, basename, &filepath);

    // Check if file exists
    if (VCWD_STAT(filepath, &statbuf) != 0 || S_ISREG(statbuf.st_mode) == 0) {
        vapor_report_error(engine, "Unable to load template file");
        efree(basename);
        efree(filepath);
        return 0;
    }

    // Check open_basedir
    if (php_check_open_basedir(filepath)) {
        vapor_report_error(engine, "open_basedir restriction in effect, unable to open file");
        efree(tplname);
        efree(filepath);
        return 0;
    }

    // Properties
    tpl->initialized = 1;
    tpl->folder      = folder;
    tpl->basename    = basename;
    tpl->filepath    = filepath;
    tpl->layout_name = NULL;
    tpl->layout_data = NULL;
    tpl->engine      = engine;

    ALLOC_HASHTABLE(tpl->sections);
    zend_hash_init(tpl->sections, 0, NULL, ZVAL_PTR_DTOR, 0);

    return 1;
}

void vapor_template_execute(vapor_template *tpl, zval *content)
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

static PHP_METHOD(Template, __construct)
{
    char *tplname, *namecopy;
    size_t len;
    vapor_engine *engine;
    vapor_template *tpl;
    zval *engine_obj;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJECT_OF_CLASS(engine_obj, vapor_ce_engine)
        Z_PARAM_STRING(tplname, len)
    ZEND_PARSE_PARAMETERS_END();

    engine = Z_VAPOR_ENGINE_P(engine_obj);

    vapor_template_instantiate(return_value);
    tpl = Z_VAPOR_TEMPLATE_P(return_value);

    if (!vapor_template_initialize(Z_VAPOR_TEMPLATE_P(return_value), engine, tplname)) {
        zval_ptr_dtor(return_value);
        RETURN_FALSE;
    }
}

static PHP_METHOD(Template, __toString)
{
    php_printf("Template::__toString invoked");
}

static PHP_METHOD(Template, path)
{
    VAPOR_TEMPLATE_GET_OBJ
    ZVAL_STRING(return_value, template->filepath);
}

static PHP_METHOD(Template, render)
{
    zend_array *data = NULL;
    zval content;

    ZEND_PARSE_PARAMETERS_START(0, 1)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_HT(data)
    ZEND_PARSE_PARAMETERS_END();

    VAPOR_TEMPLATE_GET_OBJ

    // Copy user data
    if (data != NULL) {
        vapor_data_copy(zend_rebuild_symbol_table(), data);
    }

    // Execute PHP Source
    ZVAL_UNDEF(&content);
    vapor_template_execute(template, &content);

    // Render layout template
    if (template->layout_name) {
        // Call Engine::make() to instantise a new template object
        zval obj, arg, rv;
        ZVAL_STRING(&arg, template->layout_name);
        ZVAL_OBJ(&obj, &template->engine->std);
        zend_call_method_with_1_params(&obj, template->engine->std.ce, NULL, "make", &rv, &arg);
        zval_ptr_dtor(&arg);

        // Copy section data, and clear "content"
        zval foo;
        ZVAL_ZVAL(&foo, &content, 0, 1); // move
        vapor_template *layout = Z_VAPOR_TEMPLATE_P(&rv);
        zend_hash_str_update(layout->sections, "content", strlen("content"), &foo);

        // Call Template::render() for this template
        if (template->layout_data == NULL) {
            zend_call_method_with_0_params(&rv, Z_OBJCE(rv), NULL, "render", &content);
        } else {
            zval arg1;
            ZVAL_ARR(&arg1, template->layout_data);
            zend_call_method_with_1_params(&rv, Z_OBJCE(rv), NULL, "render", &content, &arg1);
            zval_ptr_dtor(&arg1);
        }

        zval_ptr_dtor(&rv); // not &obj
    }

    ZVAL_ZVAL(return_value, &content, 0, 1);
}

static PHP_METHOD(Template, layout)
{
    char *layout;
    size_t len;
    vapor_template *tpl;
    zend_array *data = NULL;
    zval obj;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(layout, len)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_HT(data)
    ZEND_PARSE_PARAMETERS_END();

    VAPOR_TEMPLATE_GET_OBJ

    template->layout_name = estrdup(layout);
    template->layout_data = data;

    RETURN_TRUE;
}

static PHP_METHOD(Template, section)
{
    char *content, *section;
    size_t len;
    zval *tmp;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(section, len)
    ZEND_PARSE_PARAMETERS_END();

    VAPOR_TEMPLATE_GET_OBJ

    if (NULL != (tmp = zend_hash_str_find(template->sections, section, len)) && Z_TYPE_P(tmp) == IS_STRING) {
        RETURN_STRING(Z_STRVAL_P(tmp));
    }
    RETURN_NULL();
}

static PHP_METHOD(Template, insert)
{
    char *tplname;
    size_t len;
    vapor_template *tpl;
    zend_array *data = NULL;
    zval content;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(tplname, len)
        Z_PARAM_OPTIONAL
        Z_PARAM_ARRAY_HT(data)
    ZEND_PARSE_PARAMETERS_END();

    // Get $this variable
    VAPOR_TEMPLATE_GET_OBJ

    // Call Engine::make() to instantise a new template object
    zval obj, arg, rv;
    ZVAL_STRINGL(&arg, tplname, len);
    ZVAL_OBJ(&obj, &template->engine->std);
    zend_call_method_with_1_params(&obj, template->engine->std.ce, NULL, "make", &rv, &arg);
    zval_ptr_dtor(&arg);

    // Call Template::render() for this template
    if (data == NULL) {
        zend_call_method_with_0_params(&rv, Z_OBJCE(rv), NULL, "render", &content);
    } else {
        zval arg1;
        ZVAL_ARR(&arg1, data);
        zend_call_method_with_1_params(&rv, Z_OBJCE(rv), NULL, "render", &content, &arg1);
        zval_ptr_dtor(&arg1);
    }
    zval_ptr_dtor(&rv);

    // Send to output
    php_printf("%s", Z_STRVAL(content));
    zval_ptr_dtor(&content);
}

static PHP_METHOD(Template, batch)
{
}

static PHP_METHOD(Template, escape)
{
    char *str = NULL;
    size_t len;
    zend_string *callbacks = NULL, *delim, *escaped;

    ZEND_PARSE_PARAMETERS_START(1, 2)
        Z_PARAM_STRING(str, len)
        Z_PARAM_OPTIONAL
        Z_PARAM_STR(callbacks)
    ZEND_PARSE_PARAMETERS_END();

    escaped = php_escape_html_entities(str, strlen(str), 0, ENT_QUOTES | ENT_SUBSTITUTE, cs_utf_8);

    if (callbacks && ZSTR_LEN(callbacks) > 0) {
        zval used_callbacks;
        array_init(&used_callbacks);

        delim = zend_string_init("|", strlen("|"), 0);
        php_explode(delim, callbacks, &used_callbacks, VAPOR_MAX_FUNCTIONS);

        if (&used_callbacks) {
            zval *funcname;
            ZEND_HASH_FOREACH_VAL (Z_ARRVAL(used_callbacks), funcname) {
                zval *callback;

                VAPOR_TEMPLATE_GET_OBJ
                if (SUCCESS == vapor_get_callback(template->engine->functions, Z_STRVAL_P(funcname), &callback)) {
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

ZEND_BEGIN_ARG_INFO_EX(arginfo__construct, 0, 0, 2)
    ZEND_ARG_INFO(0, engine)
    ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_path, 0, 0, 0)
    ZEND_ARG_INFO(0, tplname)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_render, 0, 0, 0)
    ZEND_ARG_INFO(0, data)
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

ZEND_BEGIN_ARG_INFO_EX(arginfo_batch, 0, 0, 1)
    ZEND_ARG_INFO(0, var)
    ZEND_ARG_INFO(0, funcs)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_escape, 0, 0, 1)
    ZEND_ARG_INFO(0, var)
    ZEND_ARG_INFO(0, func)
ZEND_END_ARG_INFO()

const zend_function_entry vapor_methods_template[] = {
    PHP_ME(Template,    __construct,    arginfo__construct,     ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(Template,    __toString,     NULL,                   ZEND_ACC_PUBLIC)
    PHP_ME(Template,    path,           arginfo_path,           ZEND_ACC_PUBLIC)
    PHP_ME(Template,    render,         arginfo_render,         ZEND_ACC_PUBLIC)
    PHP_ME(Template,    layout,         arginfo_layout,         ZEND_ACC_PUBLIC)
    PHP_ME(Template,    section,        arginfo_section,        ZEND_ACC_PUBLIC)
    PHP_ME(Template,    insert,         arginfo_insert,         ZEND_ACC_PUBLIC)
    PHP_ME(Template,    batch,          arginfo_batch,          ZEND_ACC_PUBLIC)
    PHP_ME(Template,    escape,         arginfo_escape,         ZEND_ACC_PUBLIC)
    PHP_MALIAS(Template,    b,  batch,  arginfo_batch,          ZEND_ACC_PUBLIC)
    PHP_MALIAS(Template,    e,  escape, arginfo_escape,         ZEND_ACC_PUBLIC)
    PHP_FE_END
};
