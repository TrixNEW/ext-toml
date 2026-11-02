#ifndef TOML_PARSER_H
#define TOML_PARSER_H

#include "php.h"

#define TOML_MAX_DEPTH 256

typedef struct {
	zend_long line;
	zend_long column;
	char message[192];
} php_toml_error;

/* p must be followed by a '\0' so a sequence cut short stops there */
static zend_always_inline size_t php_toml_utf8_len(const unsigned char *p)
{
	unsigned c = p[0];
	if (c >= 0xC2 && c <= 0xDF) {
		return (p[1] & 0xC0) == 0x80 ? 2 : 0;
	}
	if (c >= 0xE0 && c <= 0xEF) {
		unsigned lo = c == 0xE0 ? 0xA0 : 0x80, hi = c == 0xED ? 0x9F : 0xBF;
		return p[1] >= lo && p[1] <= hi && (p[2] & 0xC0) == 0x80 ? 3 : 0;
	}
	if (c >= 0xF0 && c <= 0xF4) {
		unsigned lo = c == 0xF0 ? 0x90 : 0x80, hi = c == 0xF4 ? 0x8F : 0xBF;
		return p[1] >= lo && p[1] <= hi && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80 ? 4 : 0;
	}
	return 0;
}

/* src[len] must be '\0', which zend_strings guarantee */
bool php_toml_decode(zval *rv, const char *src, size_t len, php_toml_error *err);

#endif
