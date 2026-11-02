#ifndef TOML_ENCODER_H
#define TOML_ENCODER_H

#include "php.h"

#define PHP_TOML_ERROR_SIZE 192

/* returns NULL with the reason in error, which holds PHP_TOML_ERROR_SIZE bytes */
zend_string *php_toml_encode(HashTable *ht, char *error);

#endif
