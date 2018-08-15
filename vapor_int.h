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

extern zend_class_entry *vapor_ce_engine;
extern zend_class_entry *vapor_ce_template;
extern zend_object_handlers vapor_object_handlers_template;

extern const zend_function_entry vapor_methods_template[];

void zend_array_dump(zend_array *data);
void vapor_engine_dump(vapor_engine *val);
void vapor_template_dump(vapor_template *val);

void vapor_report_error(vapor_engine *obj, char *format, ...);
void vapor_data_copy(zend_array *symtable, zend_array *data);
int vapor_get_callback(zend_array *functions, char *func_name, zval **callback);

zend_object *vapor_template_new(zend_class_entry *ce);
void vapor_template_free_storage(zend_object *obj);
zval *vapor_object_get_property_template(zval *object, zval *member, int type, void **cache_slot, zval *rv);
void vapor_object_set_property_template(zval *object, zval *key, zval *value, void **cache_slot);
void vapor_template_parse_name(char *tplname, char **folder, char **basename);
int  vapor_template_check_folder(zend_array *folders, char *folder);
void vapor_template_filepath(vapor_engine *engine, char *folder, char *basename, char **filepath);
zval *vapor_template_instantiate(zval *object);
int  vapor_template_initialize(vapor_template *tpobj, vapor_engine *engine, char *tplname);
void vapor_template_render(vapor_template *tpl, zend_array *data, zval *content);
