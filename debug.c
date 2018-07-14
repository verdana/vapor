#include "php.h"
#include "ext/standard/php_var.h"

void zend_array_dump(zend_array *data)
{
    if (data != NULL) {
        zval tmp;
        ZVAL_ARR(&tmp, data);
        php_var_dump(&tmp, 0);
    }
}
