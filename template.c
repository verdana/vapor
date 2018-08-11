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
        *basename = tplname;
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
    object_init_ex(object, vapor_ce_template);
    return object;
}

int vapor_template_initialize(vapor_template *tpobj, vapor_engine *engine, char *tplname)
{
    char *folder = NULL, *basename = NULL, *filepath = NULL;

    // Extract foler and basename
    vapor_template_parse_name(tplname, &folder, &basename);

    // Check folder exists
    if (folder != NULL && !vapor_template_check_folder(engine->folders, folder)) {
        vapor_report_error(engine, "Folder \"%s\" does not exists", folder);
        return 0;
    }

    // Update filepath
    vapor_template_filepath(engine, folder, basename, &filepath);

    // Properties
    tpobj->initialized = 1;
    tpobj->folder      = folder;
    tpobj->basename    = basename;
    tpobj->filepath    = filepath;
    tpobj->layout      = NULL;
    tpobj->layout_data = NULL;
    tpobj->engine      = engine;

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

void vapor_template_render(vapor_engine *engine, vapor_template *tpl, zend_array *data, zval *content)
{
    zend_stat_t statbuf;

    // 检查模板结构体是否准备就绪
    // tpl 的准备工作是有可能失败的
    // 比如用户指定的 folder 或 filename 无效
    if (tpl->initialized != 1) {
        // vapor_free_template(tpl);
        return;
    }

    // 检查模板文件路径是否正确
    if (VCWD_STAT(tpl->filepath, &statbuf) != 0 || S_ISREG(statbuf.st_mode) == 0) {
        vapor_report_error(engine, "Unable to load template file");
        // vapor_free_template(tpl);
        return;
    }

    // 检查模板文件是否被 open_basedir 限制
    if (php_check_open_basedir(tpl->filepath)) {
        vapor_report_error(engine, "open_basedir restriction in effect, unable to open file");
        // vapor_free_template(tpl);
        return;
    }

    // 清理 content 已有的数据
    // 是由 php_output_get_contents 赋予的在堆上的字符串
    // 所以再次使用 content 的时候，必须保证之前的内存已经被释放
    if (content) {
        zval_ptr_dtor(content);
        ZVAL_UNDEF(content);
    }

    // 复制数据
    if (data) {
        vapor_data_copy(zend_rebuild_symbol_table(), data);
    }

    // 执行 PHP 文件
    vapor_template_execute(tpl, content);

    // 如果模板文件指定了 layout，递归调用本函数，执行 layout 文件
    if (tpl->layout) {
        zval foo;
        ZVAL_ZVAL(&foo, content, 1, 0); // just COPY
        zend_hash_str_update(engine->sections, "content", sizeof("content") - 1, &foo);

        // 初始化一个模板对象处理布局文件
        vapor_template *layout_tpl;
        zval baz;
        vapor_template_instantiate(&baz);
        layout_tpl = Z_VAPOR_TEMPLATE_P(&baz);

        if (!vapor_template_initialize(layout_tpl, engine, tpl->layout)) {
            zval_ptr_dtor(&baz);
            return;
        }

        // 继续渲染布局文件
        engine->current = layout_tpl;
        vapor_template_render(engine, layout_tpl, tpl->layout_data, content);

        // 清理
        efree(tpl->layout);
        zval_ptr_dtor(&baz);
    }
}

static PHP_METHOD(Template, __construct)
{
    char *tplname, *folder = NULL, *basename = NULL;
    size_t len;
    vapor_engine *engine;
    zval *obj;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJECT_OF_CLASS(obj, vapor_ce_engine)
        Z_PARAM_STRING(tplname, len)
    ZEND_PARSE_PARAMETERS_END();

    engine = Z_VAPOR_ENGINE_P(obj);

    VAPOR_TEMPLATE_GET_OBJ

    vapor_template_parse_name(estrdup(tplname), &folder, &basename);

    template->folder   = folder;
    template->basename = basename;
    template->engine   = engine;
}

static PHP_METHOD(Template, __toString)
{
    php_printf("Template::__toString invoked");
}

static PHP_METHOD(Template, render)
{
    php_printf("Template::render invoked");
}

ZEND_BEGIN_ARG_INFO_EX(arginfo__construct, 0, 0, 2)
    ZEND_ARG_INFO(0, engine)
    ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO()

const zend_function_entry vapor_methods_template[] = {
    PHP_ME(Template,    __construct,    arginfo__construct,     ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    PHP_ME(Template,    __toString,     NULL,                   ZEND_ACC_PUBLIC)
    PHP_FE_END
};
