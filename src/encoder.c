#include "php.h"
#include "zend_smart_str.h"
#include "encoder.h"
#include "parser.h"

typedef struct {
	smart_str out;
	smart_str path;
	char *error;
} toml_encoder;

static const char hex[] = "0123456789ABCDEF";

static zend_never_inline bool fail(toml_encoder *E, const char *fmt, const char *arg)
{
	snprintf(E->error, PHP_TOML_ERROR_SIZE, fmt, arg);
	return false;
}

static bool encode_string(toml_encoder *E, smart_str *out, const zend_string *str, const char *what)
{
	const unsigned char *p = (const unsigned char *) ZSTR_VAL(str), *end = p + ZSTR_LEN(str), *run = p;
	smart_str_alloc(out, ZSTR_LEN(str) + 2, 0);
	smart_str_appendc(out, '"');
	while (p < end) {
		unsigned c = *p;
		if (c >= 0x80) {
			size_t n = php_toml_utf8_len(p);
			if (!n || n > (size_t) (end - p)) {
				return fail(E, "Invalid UTF-8 in %s", what);
			}
			p += n;
			continue;
		}
		if (c >= 0x20 && c != 0x7F && c != '"' && c != '\\') {
			p++;
			continue;
		}
		smart_str_appendl(out, (const char *) run, p - run);
		switch (c) {
			case '"': smart_str_appendl(out, "\\\"", 2); break;
			case '\\': smart_str_appendl(out, "\\\\", 2); break;
			case '\n': smart_str_appendl(out, "\\n", 2); break;
			case '\r': smart_str_appendl(out, "\\r", 2); break;
			case '\t': smart_str_appendl(out, "\\t", 2); break;
			case '\b': smart_str_appendl(out, "\\b", 2); break;
			case '\f': smart_str_appendl(out, "\\f", 2); break;
			default: {
				char esc[6] = {'\\', 'u', '0', '0', hex[c >> 4], hex[c & 15]};
				smart_str_appendl(out, esc, sizeof(esc));
			}
		}
		run = ++p;
	}
	smart_str_appendl(out, (const char *) run, p - run);
	smart_str_appendc(out, '"');
	return true;
}

static bool encode_key(toml_encoder *E, smart_str *out, const zend_string *key, zend_ulong idx)
{
	if (!key) {
		smart_str_append_long(out, (zend_long) idx);
		return true;
	}
	size_t len = ZSTR_LEN(key);
	for (size_t i = 0; i < len; i++) {
		unsigned char c = ZSTR_VAL(key)[i];
		if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
			return encode_string(E, out, key, "key");
		}
	}
	if (len == 0) {
		smart_str_appendl(out, "\"\"", 2);
	} else {
		smart_str_append(out, key);
	}
	return true;
}

static bool is_table(zval *v)
{
	ZVAL_DEREF(v);
	return Z_TYPE_P(v) == IS_ARRAY && zend_hash_num_elements(Z_ARRVAL_P(v)) && !zend_array_is_list(Z_ARRVAL_P(v));
}

/* non-empty tables, and lists made only of them, go under headers */
static bool is_subtable(zval *v)
{
	if (Z_TYPE_P(v) != IS_ARRAY || !zend_hash_num_elements(Z_ARRVAL_P(v))) {
		return false;
	}
	if (!zend_array_is_list(Z_ARRVAL_P(v))) {
		return true;
	}
	zval *element;
	ZEND_HASH_FOREACH_VAL_IND(Z_ARRVAL_P(v), element) {
		if (!is_table(element)) {
			return false;
		}
	} ZEND_HASH_FOREACH_END();
	return true;
}

static bool encode_value(toml_encoder *E, zval *v, int depth)
{
	ZVAL_DEREF(v);
	smart_str *out = &E->out;
	switch (Z_TYPE_P(v)) {
		case IS_FALSE:
			smart_str_appendl(out, "false", 5);
			return true;
		case IS_TRUE:
			smart_str_appendl(out, "true", 4);
			return true;
		case IS_LONG:
			smart_str_append_long(out, Z_LVAL_P(v));
			return true;
		case IS_DOUBLE: {
			double d = Z_DVAL_P(v);
			if (zend_isnan(d)) {
				smart_str_appendl(out, "nan", 3);
			} else if (zend_isinf(d)) {
				smart_str_appends(out, d > 0 ? "inf" : "-inf");
			} else {
				smart_str_append_double(out, d, -1, true);
			}
			return true;
		}
		case IS_STRING:
			return encode_string(E, out, Z_STR_P(v), "string");
		case IS_ARRAY: {
			if (depth >= TOML_MAX_DEPTH) {
				return fail(E, "Nesting exceeds %s levels", ZEND_TOSTR(TOML_MAX_DEPTH));
			}
			HashTable *ht = Z_ARRVAL_P(v);
			bool list = zend_array_is_list(ht), first = true;
			zend_string *key;
			zend_ulong idx;
			zval *element;
			smart_str_appendl(out, list ? "[" : "{ ", list ? 1 : 2);
			ZEND_HASH_FOREACH_KEY_VAL_IND(ht, idx, key, element) {
				if (!first) {
					smart_str_appendl(out, ", ", 2);
				}
				first = false;
				if (!list) {
					if (!encode_key(E, out, key, idx)) {
						return false;
					}
					smart_str_appendl(out, " = ", 3);
				}
				if (!encode_value(E, element, depth + 1)) {
					return false;
				}
			} ZEND_HASH_FOREACH_END();
			smart_str_appendl(out, list ? "]" : " }", list ? 1 : 2);
			return true;
		}
		default:
			return fail(E, "Cannot encode %s to TOML", zend_zval_value_name(v));
	}
}

static void encode_header(toml_encoder *E, bool array)
{
	smart_str *out = &E->out;
	if (out->s && ZSTR_LEN(out->s)) {
		smart_str_appendc(out, '\n');
	}
	smart_str_appendl(out, "[[", array ? 2 : 1);
	smart_str_append_smart_str(out, &E->path);
	smart_str_appendl(out, "]]\n" + !array, array ? 3 : 2);
}

/* plain keys come first, since TOML gives keys to the last header seen */
static bool encode_table(toml_encoder *E, HashTable *ht, int depth)
{
	zend_string *key;
	zend_ulong idx;
	zval *v;
	if (depth >= TOML_MAX_DEPTH) {
		return fail(E, "Nesting exceeds %s levels", ZEND_TOSTR(TOML_MAX_DEPTH));
	}
	ZEND_HASH_FOREACH_KEY_VAL_IND(ht, idx, key, v) {
		ZVAL_DEREF(v);
		if (!is_subtable(v)) {
			if (!encode_key(E, &E->out, key, idx)) {
				return false;
			}
			smart_str_appendl(&E->out, " = ", 3);
			if (!encode_value(E, v, depth + 1)) {
				return false;
			}
			smart_str_appendc(&E->out, '\n');
		}
	} ZEND_HASH_FOREACH_END();

	ZEND_HASH_FOREACH_KEY_VAL_IND(ht, idx, key, v) {
		ZVAL_DEREF(v);
		if (!is_subtable(v)) {
			continue;
		}
		size_t saved = E->path.s ? ZSTR_LEN(E->path.s) : 0;
		if (saved) {
			smart_str_appendc(&E->path, '.');
		}
		if (!encode_key(E, &E->path, key, idx)) {
			return false;
		}
		HashTable *sub = Z_ARRVAL_P(v);
		if (zend_array_is_list(sub)) {
			zval *element;
			ZEND_HASH_FOREACH_VAL_IND(sub, element) {
				ZVAL_DEREF(element);
				encode_header(E, true);
				if (!encode_table(E, Z_ARRVAL_P(element), depth + 2)) {
					return false;
				}
			} ZEND_HASH_FOREACH_END();
		} else {
			encode_header(E, false);
			if (!encode_table(E, sub, depth + 1)) {
				return false;
			}
		}
		ZSTR_LEN(E->path.s) = saved;
	} ZEND_HASH_FOREACH_END();
	return true;
}

zend_string *php_toml_encode(HashTable *ht, char *error)
{
	toml_encoder E = {0};
	E.error = error;
	bool ok = encode_table(&E, ht, 0);
	smart_str_free(&E.path);
	if (!ok) {
		smart_str_free(&E.out);
		return NULL;
	}
	return smart_str_extract(&E.out);
}
