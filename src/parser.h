#ifndef TOML_PARSER_H
#define TOML_PARSER_H

#include "php.h"

#define TOML_MAX_DEPTH 256

typedef struct {
	zend_long line;
	zend_long column;
	char message[192];
} php_toml_error;

/* src[len] must be '\0', which zend_strings guarantee */
bool php_toml_decode(zval *rv, const char *src, size_t len, php_toml_error *err);

#endif
