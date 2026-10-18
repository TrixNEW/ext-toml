/*
 * TOML 1.1 parser that builds PHP arrays directly, without an intermediate tree.
 *
 * Grammar handling and validation rules are derived from tomlc17
 * (https://github.com/cktan/tomlc17, commit bbf1a3bf97d36d63744d12f4ae78047db5b662cb),
 * Copyright (c) 2024-2026 CK Tan, MIT License. See LICENSE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "parser.h"
#include "zend_strtod.h"

#if defined(ZTS) && defined(COMPILE_DL_TOML)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

/* Flags of tables and arrays that later lines may still extend. Inline tables and
 * static arrays are never registered, so a missing entry means sealed. */
enum {
	OPEN_IMPLICIT = 1, /* created on the path of a [header] */
	OPEN_DEFINED = 2,  /* named by its own [header] */
	OPEN_DOTTED = 4,   /* created or extended through a dotted key */
	OPEN_AOT = 8,      /* array of tables */
};

typedef struct {
	zend_string *str; /* NULL for integer keys */
	zend_ulong idx;
	const char *at;
	uint32_t len;
} toml_key;

typedef struct {
	const HashTable *ht;
	uint32_t flags;
} toml_open;

typedef struct {
	const char *src;
	const char *p;
	const char *end;
	const char *err_at;
	php_toml_error *err;
	uint32_t depth;

	zend_string **keys;
	uint32_t keys_mask;
	uint32_t keys_count;

	toml_open *open;
	uint32_t open_mask;
	uint32_t open_count;

	toml_key *parts;
	uint32_t parts_cap;

	char *buf;
	size_t buf_cap;

	zend_string *keys_inline[64];
	toml_open open_inline[32];
	toml_key parts_inline[16];
} toml_parser;

static bool parse_value(toml_parser *P, zval *rv);
static bool parse_keyval(toml_parser *P, HashTable *t);

static zend_always_inline bool is_digit(unsigned char c)
{
	return (unsigned) (c - '0') < 10;
}

enum { C_BARE = 1, C_BASIC = 2, C_LITERAL = 4 };

/* bare key characters, and characters that need no attention inside basic and literal strings */
static const unsigned char char_class[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	6, 6, 4, 6, 6, 6, 6, 2, 6, 6, 6, 6, 6, 7, 6, 6,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6,
	6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 6, 4, 6, 6, 7,
	6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
	7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 0,
};

static zend_always_inline bool is_bare(unsigned char c)
{
	return char_class[c] & C_BARE;
}

/* printable ASCII */
static zend_always_inline bool is_plain(unsigned char c)
{
	return (unsigned) (c - 0x20) < 0x5F;
}

static zend_always_inline unsigned hex_value(unsigned char c)
{
	if (is_digit(c)) {
		return c - '0';
	}
	c |= 0x20;
	return (unsigned) (c - 'a') < 6 ? c - 'a' + 10 : 16;
}

static zend_always_inline bool lit_match(const char *p, const char *lit, size_t n)
{
	/* stops at the first mismatch, so the NUL sentinel is never passed */
	for (size_t i = 0; i < n; i++) {
		if (p[i] != lit[i]) {
			return false;
		}
	}
	return true;
}

static zend_never_inline ZEND_COLD bool fail(toml_parser *P, const char *at, const char *fmt, ...)
{
	if (!P->err_at) {
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(P->err->message, sizeof(P->err->message), fmt, ap);
		va_end(ap);
		P->err_at = at;
	}
	return false;
}

static zend_never_inline ZEND_COLD bool fail_unexpected(toml_parser *P, const char *at, const char *expected)
{
	unsigned char c = (unsigned char) *at;
	char what[16];

	if (at >= P->end) {
		strcpy(what, "end of input");
	} else if (c == '\n' || (c == '\r' && at[1] == '\n')) {
		strcpy(what, "newline");
	} else if (c > 0x20 && c < 0x7F) {
		snprintf(what, sizeof(what), "'%c'", c);
	} else if (c == ' ') {
		strcpy(what, "space");
	} else {
		snprintf(what, sizeof(what), "byte 0x%02X", c);
	}
	return fail(P, at, "Unexpected %s, expected %s", what, expected);
}

/* fmt takes the key as written, quoted keys keep their own quotes */
static zend_never_inline ZEND_COLD bool fail_key(toml_parser *P, const toml_key *k, const char *fmt)
{
	char name[80];
	int len = (int) MIN(k->len, 64);
	if (*k->at == '"' || *k->at == '\'') {
		snprintf(name, sizeof(name), "%.*s", len, k->at);
	} else {
		snprintf(name, sizeof(name), "'%.*s'", len, k->at);
	}
	return fail(P, k->at, fmt, name);
}

/* Length of the valid UTF-8 sequence at p (p[0] >= 0x80), or 0. */
static zend_always_inline size_t utf8_len(const unsigned char *p)
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

static zend_never_inline bool skip_char(toml_parser *P, const unsigned char **pp, const char *what)
{
	const unsigned char *p = *pp;
	if (*p >= 0x80) {
		size_t n = utf8_len(p);
		if (n) {
			*pp = p + n;
			return true;
		}
		return fail(P, (const char *) p, "Invalid UTF-8 in %s", what);
	}
	if ((const char *) p >= P->end) {
		return fail(P, (const char *) p, "Unterminated %s", what);
	}
	return fail(P, (const char *) p, "Control character U+%04X is not allowed in %s", *p, what);
}

static zend_always_inline void skip_ws(toml_parser *P)
{
	const char *p = P->p;
	while (*p == ' ' || *p == '\t') {
		p++;
	}
	P->p = p;
}

static bool skip_comment(toml_parser *P)
{
	const unsigned char *p = (const unsigned char *) P->p + 1;
	for (;;) {
		unsigned char c = *p;
		if (EXPECTED(is_plain(c) || c == '\t')) {
			p++;
		} else if (c == '\n' || (c == '\r' && p[1] == '\n') || (const char *) p == P->end) {
			break;
		} else if (!skip_char(P, &p, "comment")) {
			return false;
		}
	}
	P->p = (const char *) p;
	return true;
}

/* whitespace, newlines and comments, as allowed inside arrays and inline tables */
static bool skip_blank(toml_parser *P)
{
	for (;;) {
		char c = *P->p;
		if (c == ' ' || c == '\t' || c == '\n') {
			P->p++;
		} else if (c == '\r' && P->p[1] == '\n') {
			P->p += 2;
		} else if (c == '#') {
			if (!skip_comment(P)) {
				return false;
			}
		} else {
			return true;
		}
	}
}

static bool expect_eol(toml_parser *P)
{
	skip_ws(P);
	if (*P->p == '#' && !skip_comment(P)) {
		return false;
	}
	if (*P->p == '\n') {
		P->p++;
		return true;
	}
	if (*P->p == '\r' && P->p[1] == '\n') {
		P->p += 2;
		return true;
	}
	if (P->p == P->end) {
		return true;
	}
	return fail_unexpected(P, P->p, "a newline");
}

static char *scratch(toml_parser *P, size_t n)
{
	if (n > P->buf_cap) {
		P->buf_cap = MAX(n, P->buf_cap * 2);
		P->buf = erealloc(P->buf, P->buf_cap);
	}
	return P->buf;
}

static void keys_grow(toml_parser *P)
{
	uint32_t old_mask = P->keys_mask, mask = old_mask * 2 + 1;
	zend_string **old = P->keys, **keys = ecalloc(mask + 1, sizeof(*keys));
	for (uint32_t i = 0; i <= old_mask; i++) {
		if (old[i]) {
			uint32_t j = (uint32_t) ZSTR_H(old[i]) & mask;
			while (keys[j]) {
				j = (j + 1) & mask;
			}
			keys[j] = old[i];
		}
	}
	if (old != P->keys_inline) {
		efree(old);
	}
	P->keys = keys;
	P->keys_mask = mask;
}

/* Keys repeat heavily (arrays of tables), so each distinct key is allocated once
 * per parse and shared by every table that uses it. */
static zend_string *key_string(toml_parser *P, const char *s, size_t len, zend_ulong h)
{
	if (len <= 1) {
		return zend_string_init_fast(s, len);
	}
	uint32_t i = (uint32_t) h & P->keys_mask;
	zend_string *k;
	while ((k = P->keys[i])) {
		if (ZSTR_H(k) == h && ZSTR_LEN(k) == len && !memcmp(ZSTR_VAL(k), s, len)) {
			return k;
		}
		i = (i + 1) & P->keys_mask;
	}
	k = zend_string_init(s, len, 0);
	ZSTR_H(k) = h;
	P->keys[i] = k;
	if (++P->keys_count * 2 > P->keys_mask) {
		keys_grow(P);
	}
	return k;
}

static zend_always_inline uint32_t open_slot(const toml_parser *P, const HashTable *ht)
{
	return (uint32_t) (((uintptr_t) ht >> 3) * 0x9E3779B1u) & P->open_mask;
}

static uint32_t open_get(const toml_parser *P, const HashTable *ht)
{
	for (uint32_t i = open_slot(P, ht);; i = (i + 1) & P->open_mask) {
		if (P->open[i].ht == ht) {
			return P->open[i].flags;
		}
		if (!P->open[i].ht) {
			return 0;
		}
	}
}

static void open_set(toml_parser *P, const HashTable *ht, uint32_t flags)
{
	uint32_t i = open_slot(P, ht);
	while (P->open[i].ht && P->open[i].ht != ht) {
		i = (i + 1) & P->open_mask;
	}
	if (P->open[i].ht) {
		P->open[i].flags = flags;
		return;
	}
	P->open[i].ht = ht;
	P->open[i].flags = flags;
	if (++P->open_count * 2 > P->open_mask) {
		uint32_t old_mask = P->open_mask;
		toml_open *old = P->open;
		P->open_mask = old_mask * 2 + 1;
		P->open = ecalloc(P->open_mask + 1, sizeof(*P->open));
		for (uint32_t j = 0; j <= old_mask; j++) {
			if (old[j].ht) {
				uint32_t k = open_slot(P, old[j].ht);
				while (P->open[k].ht) {
					k = (k + 1) & P->open_mask;
				}
				P->open[k] = old[j];
			}
		}
		if (old != P->open_inline) {
			efree(old);
		}
	}
}

static zend_always_inline zval *tab_find(HashTable *t, const toml_key *k)
{
	return k->str ? zend_hash_find_known_hash(t, k->str) : zend_hash_index_find(t, k->idx);
}

static zend_always_inline zval *tab_add(HashTable *t, const toml_key *k, zval *v)
{
	return k->str ? zend_hash_add(t, k->str, v) : zend_hash_index_add(t, k->idx, v);
}

static zend_always_inline void tab_add_new(HashTable *t, const toml_key *k, zval *v)
{
	if (k->str) {
		zend_hash_add_new(t, k->str, v);
	} else {
		zend_hash_index_add_new(t, k->idx, v);
	}
}

/* only for keys known to be absent */
static HashTable *tab_new_table(toml_parser *P, HashTable *t, const toml_key *k, uint32_t flags)
{
	zval zv;
	array_init(&zv);
	tab_add_new(t, k, &zv);
	if (flags) {
		open_set(P, Z_ARRVAL(zv), flags);
	}
	return Z_ARRVAL(zv);
}

/* Length of the escape sequence at p (p[0] == '\\'), validating it. */
static size_t escape_len(toml_parser *P, const unsigned char *p)
{
	unsigned n;
	switch (p[1]) {
	case 'b': case 't': case 'n': case 'f': case 'r': case 'e': case '"': case '\\':
		return 2;
	case 'x':
		n = 2;
		break;
	case 'u':
		n = 4;
		break;
	case 'U':
		n = 8;
		break;
	default:
		return fail(P, (const char *) p, "Invalid escape sequence");
	}
	uint32_t cp = 0;
	for (unsigned i = 0; i < n; i++) {
		unsigned d = hex_value(p[2 + i]);
		if (d > 15) {
			return fail(P, (const char *) p, "Expected %u hex digits after \\%c", n, p[1]);
		}
		cp = cp << 4 | d;
	}
	if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
		return fail(P, (const char *) p, "Escape \\%c%.*s is not a Unicode scalar value", p[1], (int) n, p + 2);
	}
	return n + 2;
}

static size_t utf8_encode(char *dst, uint32_t cp)
{
	if (cp < 0x80) {
		dst[0] = (char) cp;
		return 1;
	}
	if (cp < 0x800) {
		dst[0] = (char) (0xC0 | cp >> 6);
		dst[1] = (char) (0x80 | (cp & 0x3F));
		return 2;
	}
	if (cp < 0x10000) {
		dst[0] = (char) (0xE0 | cp >> 12);
		dst[1] = (char) (0x80 | (cp >> 6 & 0x3F));
		dst[2] = (char) (0x80 | (cp & 0x3F));
		return 3;
	}
	dst[0] = (char) (0xF0 | cp >> 18);
	dst[1] = (char) (0x80 | (cp >> 12 & 0x3F));
	dst[2] = (char) (0x80 | (cp >> 6 & 0x3F));
	dst[3] = (char) (0x80 | (cp & 0x3F));
	return 4;
}

/* Decodes an already validated basic string body. The result is never longer than the input. */
static size_t unescape(char *dst, const char *s, const char *end, const char *esc)
{
	char *d = dst;
	memcpy(d, s, esc - s);
	d += esc - s;
	s = esc;
	while (s < end) {
		if (*s != '\\') {
			*d++ = *s++;
			continue;
		}
		unsigned n = 0;
		switch (s[1]) {
		case 'b': *d++ = '\b'; s += 2; continue;
		case 't': *d++ = '\t'; s += 2; continue;
		case 'n': *d++ = '\n'; s += 2; continue;
		case 'f': *d++ = '\f'; s += 2; continue;
		case 'r': *d++ = '\r'; s += 2; continue;
		case 'e': *d++ = '\x1B'; s += 2; continue;
		case '"': *d++ = '"'; s += 2; continue;
		case '\\': *d++ = '\\'; s += 2; continue;
		case 'x': n = 2; break;
		case 'u': n = 4; break;
		case 'U': n = 8; break;
		default:
			/* line ending backslash: drop it and all whitespace up to the next content */
			s++;
			while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') {
				s++;
			}
			continue;
		}
		uint32_t cp = 0;
		for (unsigned i = 0; i < n; i++) {
			cp = cp << 4 | hex_value(s[2 + i]);
		}
		d += utf8_encode(d, cp);
		s += n + 2;
	}
	return d - dst;
}

/* Scans a basic string starting at its opening quote. *esc is the first backslash or NULL. */
static bool scan_basic(toml_parser *P, const char **start, size_t *len, const char **esc)
{
	const unsigned char *p = (const unsigned char *) P->p + 1;
	const char *first = NULL;
	*start = (const char *) p;
	for (;;) {
		unsigned char c = *p;
		if (EXPECTED(char_class[c] & C_BASIC)) {
			p++;
		} else if (c == '"') {
			break;
		} else if (c == '\\') {
			size_t n = escape_len(P, p);
			if (!n) {
				return false;
			}
			if (!first) {
				first = (const char *) p;
			}
			p += n;
		} else if (c == '\t') {
			p++;
		} else if (c == '\n' || c == '\r') {
			return fail(P, (const char *) p, "Unterminated string");
		} else if (!skip_char(P, &p, "string")) {
			return false;
		}
	}
	*len = (const char *) p - *start;
	*esc = first;
	P->p = (const char *) p + 1;
	return true;
}

static bool scan_literal(toml_parser *P, const char **start, size_t *len)
{
	const unsigned char *p = (const unsigned char *) P->p + 1;
	*start = (const char *) p;
	for (;;) {
		unsigned char c = *p;
		if (EXPECTED(char_class[c] & C_LITERAL)) {
			p++;
		} else if (c == '\'') {
			break;
		} else if (c == '\t') {
			p++;
		} else if (c == '\n' || c == '\r') {
			return fail(P, (const char *) p, "Unterminated string");
		} else if (!skip_char(P, &p, "string")) {
			return false;
		}
	}
	*len = (const char *) p - *start;
	P->p = (const char *) p + 1;
	return true;
}

/* Scans a multi-line string body after its opening delimiter. Up to two quotes
 * right before the closing delimiter belong to the content. */
static bool scan_multiline(toml_parser *P, char q, const char **start, size_t *len, const char **esc)
{
	const unsigned char *p = (const unsigned char *) P->p + 3;
	const char *first = NULL;
	unsigned char plain = q == '"' ? C_BASIC : C_LITERAL;
	if (*p == '\n') {
		p++;
	} else if (*p == '\r' && p[1] == '\n') {
		p += 2;
	}
	*start = (const char *) p;
	for (;;) {
		unsigned char c = *p;
		if (EXPECTED(char_class[c] & plain)) {
			p++;
		} else if (c == q) {
			if (p[1] == q && p[2] == q) {
				if (p[3] == q) {
					p++;
					if (p[3] == q) {
						p++;
						if (p[3] == q) {
							return fail(P, (const char *) p, "Too many quotes at the end of a multi-line string");
						}
					}
				}
				break;
			}
			p++;
		} else if (c == '\\') {
			if (!first) {
				first = (const char *) p;
			}
			const unsigned char *s = p + 1;
			while (*s == ' ' || *s == '\t') {
				s++;
			}
			if (*s == '\n' || (*s == '\r' && s[1] == '\n')) {
				for (;;) {
					if (*s == ' ' || *s == '\t' || *s == '\n') {
						s++;
					} else if (*s == '\r' && s[1] == '\n') {
						s += 2;
					} else {
						break;
					}
				}
				p = s;
				continue;
			}
			size_t n = escape_len(P, p);
			if (!n) {
				return false;
			}
			p += n;
		} else if (c == '\t' || c == '\n') {
			p++;
		} else if (c == '\r' && p[1] == '\n') {
			p += 2;
		} else if (!skip_char(P, &p, "multi-line string")) {
			return false;
		}
	}
	*len = (const char *) p - *start;
	*esc = first;
	P->p = (const char *) p + 3;
	return true;
}

static void make_string(zval *rv, const char *s, size_t len, const char *esc)
{
	if (!esc) {
		ZVAL_STRINGL_FAST(rv, s, len);
		return;
	}
	zend_string *str = zend_string_alloc(len, 0);
	size_t n = unescape(ZSTR_VAL(str), s, s + len, esc);
	if (n <= 1) {
		char c = ZSTR_VAL(str)[0];
		zend_string_efree(str);
		if (n) {
			ZVAL_CHAR(rv, c);
		} else {
			ZVAL_EMPTY_STRING(rv);
		}
		return;
	}
	if (len - n > 64 && n < len / 2) {
		str = zend_string_truncate(str, n, 0);
	}
	ZSTR_LEN(str) = n;
	ZSTR_VAL(str)[n] = '\0';
	ZVAL_NEW_STR(rv, str);
}

static bool parse_string(toml_parser *P, zval *rv)
{
	const char *s, *esc = NULL;
	size_t len;
	char q = *P->p;
	if (P->p[1] == q && P->p[2] == q) {
		if (!scan_multiline(P, q, &s, &len, &esc)) {
			return false;
		}
	} else if (q == '"') {
		if (!scan_basic(P, &s, &len, &esc)) {
			return false;
		}
	} else if (!scan_literal(P, &s, &len)) {
		return false;
	}
	make_string(rv, s, len, esc);
	return true;
}

static bool parse_key_part(toml_parser *P, toml_key *k)
{
	const char *s, *esc = NULL;
	size_t len;
	zend_ulong h, idx;

	k->at = P->p;
	if (is_bare(*P->p)) {
		/* bare keys are ASCII, so this matches zend_inline_hash_func() */
		const unsigned char *p = (const unsigned char *) P->p;
		h = Z_UL(5381);
		do {
			h = h * 33 + *p++;
		} while (is_bare(*p));
		h |= (zend_ulong) 1 << (SIZEOF_ZEND_LONG * 8 - 1);
		s = P->p;
		len = (const char *) p - s;
		P->p = (const char *) p;
	} else if (*P->p == '"' || *P->p == '\'') {
		char q = *P->p;
		if (P->p[1] == q && P->p[2] == q) {
			return fail(P, P->p, "Multi-line strings cannot be used as keys");
		}
		if (q == '"' ? !scan_basic(P, &s, &len, &esc) : !scan_literal(P, &s, &len)) {
			return false;
		}
		if (esc) {
			char *buf = scratch(P, len);
			len = unescape(buf, s, s + len, esc);
			s = buf;
		}
		h = zend_inline_hash_func(s, len);
	} else {
		return fail_unexpected(P, P->p, "a key");
	}
	k->len = (uint32_t) MIN(P->p - k->at, UINT32_MAX);
	if (ZEND_HANDLE_NUMERIC_STR(s, len, idx)) {
		k->str = NULL;
		k->idx = idx;
	} else {
		k->str = key_string(P, s, len, h);
	}
	return true;
}

static bool parse_key(toml_parser *P, uint32_t *count)
{
	uint32_t n = 0;
	for (;;) {
		if (n == P->parts_cap) {
			toml_key *parts = safe_emalloc(P->parts_cap, 2 * sizeof(*parts), 0);
			memcpy(parts, P->parts, n * sizeof(*parts));
			if (P->parts != P->parts_inline) {
				efree(P->parts);
			}
			P->parts = parts;
			P->parts_cap *= 2;
		}
		if (!parse_key_part(P, &P->parts[n++])) {
			return false;
		}
		skip_ws(P);
		if (*P->p != '.') {
			break;
		}
		P->p++;
		skip_ws(P);
	}
	*count = n;
	return true;
}

static bool parse_number(toml_parser *P, zval *rv)
{
	const char *s = P->p, *p = s;
	bool neg = false;

	if (*p == '+' || *p == '-') {
		neg = *p++ == '-';
	}
	if (lit_match(p, "inf", 3)) {
		ZVAL_DOUBLE(rv, neg ? -ZEND_INFINITY : ZEND_INFINITY);
		P->p = p + 3;
		return true;
	}
	if (lit_match(p, "nan", 3)) {
		ZVAL_DOUBLE(rv, ZEND_NAN);
		P->p = p + 3;
		return true;
	}

	if (p[0] == '0' && (p[1] == 'x' || p[1] == 'o' || p[1] == 'b')) {
		unsigned shift = p[1] == 'x' ? 4 : p[1] == 'o' ? 3 : 1, base = 1u << shift;
		uint64_t v = 0;
		if (p != s) {
			return fail(P, s, "Hexadecimal, octal and binary integers cannot have a sign");
		}
		p += 2;
		if (hex_value(*p) >= base) {
			return fail(P, s, "Expected digits after the base prefix");
		}
		for (;;) {
			unsigned d = hex_value(*p);
			if (d < base) {
				if (v > (UINT64_MAX >> shift)) {
					return fail(P, s, "Integer is out of range");
				}
				v = v << shift | d;
				p++;
			} else if (*p == '_' && hex_value(p[1]) < base) {
				p++;
			} else {
				break;
			}
		}
		if (v > (uint64_t) ZEND_LONG_MAX) {
			return fail(P, s, "Integer is out of range");
		}
		ZVAL_LONG(rv, (zend_long) v);
		P->p = p;
		return true;
	}

	if (!is_digit(*p)) {
		return fail_unexpected(P, p, "a value");
	}
	if (*p == '0' && (is_digit(p[1]) || p[1] == '_')) {
		return fail(P, s, "Leading zeros are not allowed");
	}

	uint64_t v = 0;
	bool overflow = false;
	for (;;) {
		if (is_digit(*p)) {
			unsigned d = *p++ - '0';
			if (v > (UINT64_MAX - d) / 10) {
				overflow = true;
			} else {
				v = v * 10 + d;
			}
		} else if (*p == '_' && is_digit(p[1])) {
			p++;
		} else {
			break;
		}
	}
	if (*p == '_') {
		return fail(P, p, "Underscores must be surrounded by digits");
	}

	if (*p == '.' || *p == 'e' || *p == 'E') {
		if (*p == '.') {
			p++;
			if (!is_digit(*p)) {
				return fail(P, p, "Expected digits after the decimal point");
			}
			while (is_digit(*p) || (*p == '_' && is_digit(p[1]))) {
				p++;
			}
		}
		if (*p == 'e' || *p == 'E') {
			p++;
			if (*p == '+' || *p == '-') {
				p++;
			}
			if (!is_digit(*p)) {
				return fail(P, p, "Expected digits in the exponent");
			}
			while (is_digit(*p) || (*p == '_' && is_digit(p[1]))) {
				p++;
			}
		}
		if (*p == '_') {
			return fail(P, p, "Underscores must be surrounded by digits");
		}
		if (*p == '.') {
			return fail_unexpected(P, p, "a newline");
		}

		char small[64], *buf = (size_t) (p - s) < sizeof(small) ? small : scratch(P, p - s + 1);
		size_t n = 0;
		for (const char *q = s; q < p; q++) {
			if (*q != '_') {
				buf[n++] = *q;
			}
		}
		buf[n] = '\0';
		double d = zend_strtod(buf, NULL);
		if (zend_isinf(d)) {
			return fail(P, s, "Float is out of range");
		}
		ZVAL_DOUBLE(rv, d);
		P->p = p;
		return true;
	}

	uint64_t limit = neg ? (uint64_t) ZEND_LONG_MAX + 1 : (uint64_t) ZEND_LONG_MAX;
	if (overflow || v > limit) {
		return fail(P, s, "Integer is out of range");
	}
	ZVAL_LONG(rv, neg ? (v == limit ? ZEND_LONG_MIN : -(zend_long) v) : (zend_long) v);
	P->p = p;
	return true;
}

static zend_always_inline bool digits2(const char *p, int *v)
{
	if (is_digit(p[0]) && is_digit(p[1])) {
		*v = (p[0] - '0') * 10 + (p[1] - '0');
		return true;
	}
	return false;
}

/* Times and dates come back as RFC 3339 strings: 'T' separator, uppercase 'Z',
 * seconds filled in when omitted, fractional digits kept as written. */
static bool parse_datetime(toml_parser *P, zval *rv)
{
	const char *s = P->p, *p = s, *date = NULL, *time = NULL, *frac = NULL, *off = NULL;
	size_t frac_len = 0, off_len = 0;
	int y1, y2, mon, day, hour = 0, min = 0, sec = 0;
	bool has_sec = false;

	if (!(is_digit(p[0]) && is_digit(p[1]) && p[2] == ':')) {
		if (!(digits2(p, &y1) && digits2(p + 2, &y2) && p[4] == '-' && digits2(p + 5, &mon) && p[7] == '-' && digits2(p + 8, &day))) {
			return fail(P, s, "Invalid date, expected YYYY-MM-DD");
		}
		int year = y1 * 100 + y2;
		static const int mdays[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
		bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
		if (mon < 1 || mon > 12 || day < 1 || day > mdays[mon - 1] || (mon == 2 && day == 29 && !leap)) {
			return fail(P, s, "Invalid date %.10s", s);
		}
		date = p;
		p += 10;
		if (!((*p == 'T' || *p == 't' || *p == ' ') && is_digit(p[1]) && is_digit(p[2]) && p[3] == ':')) {
			goto done;
		}
		p++;
	}

	time = p;
	if (!(digits2(p, &hour) && p[2] == ':' && digits2(p + 3, &min))) {
		return fail(P, p, "Invalid time, expected HH:MM[:SS]");
	}
	p += 5;
	if (*p == ':') {
		if (!digits2(p + 1, &sec)) {
			return fail(P, p, "Invalid seconds");
		}
		has_sec = true;
		p += 3;
		if (*p == '.') {
			frac = p++;
			if (!is_digit(*p)) {
				return fail(P, p, "Expected digits after the decimal point");
			}
			while (is_digit(*p)) {
				p++;
			}
			frac_len = p - frac;
		}
	}
	if (hour > 23 || min > 59 || sec > 60) {
		return fail(P, time, "Invalid time %.*s", (int) (p - time), time);
	}
	if (date) {
		int oh, om;
		if (*p == 'Z' || *p == 'z') {
			off = p++;
			off_len = 1;
		} else if ((*p == '+' || *p == '-') && digits2(p + 1, &oh) && p[3] == ':' && digits2(p + 4, &om)) {
			if (oh > 23 || om > 59) {
				return fail(P, p, "Invalid time offset %.6s", p);
			}
			off = p;
			off_len = 6;
			p += 6;
		}
	}

done:;
	size_t n = (date ? 10 : 0) + (date && time ? 1 : 0) + (time ? 8 : 0) + frac_len + off_len, i = 0;
	zend_string *str = zend_string_alloc(n, 0);
	char *d = ZSTR_VAL(str);
	if (date) {
		memcpy(d, date, 10);
		i = 10;
		if (time) {
			d[i++] = 'T';
		}
	}
	if (time) {
		memcpy(d + i, time, 5);
		if (has_sec) {
			memcpy(d + i + 5, time + 5, 3);
		} else {
			memcpy(d + i + 5, ":00", 3);
		}
		i += 8;
		memcpy(d + i, frac, frac_len);
		i += frac_len;
		if (off_len == 1) {
			d[i++] = 'Z';
		} else if (off_len) {
			memcpy(d + i, off, off_len);
			i += off_len;
		}
	}
	d[i] = '\0';
	ZVAL_NEW_STR(rv, str);
	P->p = p;
	return true;
}

static bool enter(toml_parser *P)
{
	if (++P->depth > TOML_MAX_DEPTH) {
		return fail(P, P->p, "Nesting is deeper than %d levels", TOML_MAX_DEPTH);
	}
#ifdef ZEND_CHECK_STACK_LIMIT
	if (UNEXPECTED(zend_call_stack_overflowed(EG(stack_limit)))) {
		return fail(P, P->p, "Nesting is too deep for the available stack");
	}
#endif
	return true;
}

/* On failure the partly built value is released, so callers only own successes. */
static bool parse_array(toml_parser *P, zval *rv)
{
	if (!enter(P)) {
		return false;
	}
	P->p++;
	array_init(rv);
	zend_hash_real_init_packed(Z_ARRVAL_P(rv));
	for (;;) {
		if (!skip_blank(P)) {
			goto fail;
		}
		if (*P->p == ']') {
			break;
		}
		zval v;
		if (!parse_value(P, &v)) {
			goto fail;
		}
		zend_hash_next_index_insert_new(Z_ARRVAL_P(rv), &v);
		if (!skip_blank(P)) {
			goto fail;
		}
		if (*P->p == ',') {
			P->p++;
		} else if (*P->p != ']') {
			fail_unexpected(P, P->p, "',' or ']'");
			goto fail;
		}
	}
	P->p++;
	P->depth--;
	return true;

fail:
	zval_ptr_dtor_nogc(rv);
	return false;
}

static bool parse_inline_table(toml_parser *P, zval *rv)
{
	if (!enter(P)) {
		return false;
	}
	P->p++;
	array_init(rv);
	for (;;) {
		if (!skip_blank(P)) {
			goto fail;
		}
		if (*P->p == '}') {
			break;
		}
		if (!parse_keyval(P, Z_ARRVAL_P(rv)) || !skip_blank(P)) {
			goto fail;
		}
		if (*P->p == ',') {
			P->p++;
		} else if (*P->p != '}') {
			fail_unexpected(P, P->p, "',' or '}'");
			goto fail;
		}
	}
	P->p++;
	P->depth--;
	return true;

fail:
	zval_ptr_dtor_nogc(rv);
	return false;
}

static bool parse_value(toml_parser *P, zval *rv)
{
	const char *p = P->p;
	switch (*p) {
	case '"':
	case '\'':
		return parse_string(P, rv);
	case '[':
		return parse_array(P, rv);
	case '{':
		return parse_inline_table(P, rv);
	case 't':
		if (lit_match(p, "true", 4)) {
			ZVAL_TRUE(rv);
			P->p += 4;
			return true;
		}
		break;
	case 'f':
		if (lit_match(p, "false", 5)) {
			ZVAL_FALSE(rv);
			P->p += 5;
			return true;
		}
		break;
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		if (is_digit(p[1]) && (p[2] == ':' || (is_digit(p[2]) && is_digit(p[3]) && p[4] == '-'))) {
			return parse_datetime(P, rv);
		}
		ZEND_FALLTHROUGH;
	case '+': case '-': case 'i': case 'n':
		return parse_number(P, rv);
	}
	return fail_unexpected(P, p, "a value");
}

/* Follows one dotted key part of a key/value pair, creating the table if needed. */
static HashTable *descend_dotted(toml_parser *P, HashTable *t, const toml_key *k)
{
	zval *zv = tab_find(t, k);
	if (!zv) {
		return tab_new_table(P, t, k, OPEN_DOTTED);
	}
	if (Z_TYPE_P(zv) == IS_ARRAY) {
		uint32_t flags = open_get(P, Z_ARRVAL_P(zv));
		if (flags & OPEN_AOT) {
			fail_key(P, k, "Cannot add keys to array of tables %s with a dotted key");
			return NULL;
		}
		if (flags & OPEN_DEFINED) {
			fail_key(P, k, "Cannot add keys to table %s with a dotted key after its [header]");
			return NULL;
		}
		if (flags) {
			if (!(flags & OPEN_DOTTED)) {
				open_set(P, Z_ARRVAL_P(zv), flags | OPEN_DOTTED);
			}
			return Z_ARRVAL_P(zv);
		}
		fail_key(P, k, "Cannot extend inline table or array %s");
		return NULL;
	}
	fail_key(P, k, "Key %s already has a value");
	return NULL;
}

static bool parse_keyval(toml_parser *P, HashTable *t)
{
	uint32_t n;
	if (!parse_key(P, &n)) {
		return false;
	}
	if (*P->p != '=') {
		return fail_unexpected(P, P->p, "'='");
	}
	P->p++;
	skip_ws(P);
	for (uint32_t i = 0; i + 1 < n; i++) {
		if (!(t = descend_dotted(P, t, &P->parts[i]))) {
			return false;
		}
	}
	/* parse_value reuses the parts buffer for nested keys */
	toml_key last = P->parts[n - 1];
	zval v;
	if (!parse_value(P, &v)) {
		return false;
	}
	if (!tab_add(t, &last, &v)) {
		zval_ptr_dtor_nogc(&v);
		return fail_key(P, &last, "Duplicate key %s");
	}
	return true;
}

/* Follows one non-final part of a [header] path, creating implicit tables. */
static HashTable *descend_header(toml_parser *P, HashTable *t, const toml_key *k)
{
	zval *zv = tab_find(t, k);
	if (!zv) {
		return tab_new_table(P, t, k, OPEN_IMPLICIT);
	}
	if (Z_TYPE_P(zv) == IS_ARRAY) {
		HashTable *ht = Z_ARRVAL_P(zv);
		uint32_t flags = open_get(P, ht);
		if (flags & OPEN_AOT) {
			return Z_ARRVAL_P(zend_hash_index_find(ht, zend_hash_num_elements(ht) - 1));
		}
		if (flags) {
			return ht;
		}
		fail_key(P, k, "Cannot extend inline table or array %s");
		return NULL;
	}
	fail_key(P, k, "Key %s already has a value");
	return NULL;
}

static bool parse_header(toml_parser *P, HashTable *root, HashTable **cur)
{
	bool aot = P->p[1] == '[';
	uint32_t n;

	P->p += aot ? 2 : 1;
	skip_ws(P);
	if (!parse_key(P, &n)) {
		return false;
	}
	if (*P->p != ']' || (aot && P->p[1] != ']')) {
		return fail_unexpected(P, P->p + (*P->p == ']'), aot ? "']]'" : "']'");
	}
	P->p += aot ? 2 : 1;

	HashTable *t = root;
	for (uint32_t i = 0; i + 1 < n; i++) {
		if (!(t = descend_header(P, t, &P->parts[i]))) {
			return false;
		}
	}

	const toml_key *k = &P->parts[n - 1];
	zval *zv = tab_find(t, k);
	if (aot) {
		HashTable *arr;
		if (!zv) {
			zval tmp;
			array_init(&tmp);
			zend_hash_real_init_packed(Z_ARRVAL(tmp));
			tab_add_new(t, k, &tmp);
			arr = Z_ARRVAL(tmp);
			open_set(P, arr, OPEN_AOT);
		} else if (Z_TYPE_P(zv) == IS_ARRAY && (open_get(P, Z_ARRVAL_P(zv)) & OPEN_AOT)) {
			arr = Z_ARRVAL_P(zv);
		} else {
			return fail_key(P, k, "Cannot redefine %s as an array of tables");
		}
		zval tmp;
		array_init(&tmp);
		zend_hash_next_index_insert_new(arr, &tmp);
		*cur = Z_ARRVAL(tmp);
		return true;
	}

	if (!zv) {
		*cur = tab_new_table(P, t, k, OPEN_DEFINED);
		return true;
	}
	if (Z_TYPE_P(zv) == IS_ARRAY) {
		uint32_t flags = open_get(P, Z_ARRVAL_P(zv));
		if (flags == OPEN_IMPLICIT) {
			open_set(P, Z_ARRVAL_P(zv), OPEN_IMPLICIT | OPEN_DEFINED);
			*cur = Z_ARRVAL_P(zv);
			return true;
		}
		if (flags & OPEN_AOT) {
			return fail_key(P, k, "Cannot redefine array of tables %s as a table");
		}
		if (flags & OPEN_DEFINED) {
			return fail_key(P, k, "Table %s is defined more than once");
		}
		if (flags & OPEN_DOTTED) {
			return fail_key(P, k, "Table %s was already defined with dotted keys");
		}
		return fail_key(P, k, "Cannot redefine inline table or array %s");
	}
	return fail_key(P, k, "Key %s already has a value");
}

static bool parse_document(toml_parser *P, HashTable *root)
{
	HashTable *cur = root;
	for (;;) {
		skip_ws(P);
		switch (*P->p) {
		case '\n':
			P->p++;
			continue;
		case '\r':
			if (P->p[1] != '\n') {
				return fail(P, P->p, "Carriage return must be followed by a newline");
			}
			P->p += 2;
			continue;
		case '#':
			if (!skip_comment(P)) {
				return false;
			}
			continue;
		case '\0':
			if (P->p == P->end) {
				return true;
			}
			return fail(P, P->p, "NUL bytes are not allowed");
		case '[':
			if (!parse_header(P, root, &cur)) {
				return false;
			}
			break;
		default:
			if (!parse_keyval(P, cur)) {
				return false;
			}
			break;
		}
		if (!expect_eol(P)) {
			return false;
		}
	}
}

bool php_toml_decode(zval *rv, const char *src, size_t len, php_toml_error *err)
{
	toml_parser P;
	bool ok;

	P.src = src;
	P.p = src;
	P.end = src + len;
	P.err_at = NULL;
	P.err = err;
	P.depth = 0;
	P.keys = P.keys_inline;
	P.keys_mask = sizeof(P.keys_inline) / sizeof(P.keys_inline[0]) - 1;
	P.keys_count = 0;
	memset(P.keys_inline, 0, sizeof(P.keys_inline));
	P.open = P.open_inline;
	P.open_mask = sizeof(P.open_inline) / sizeof(P.open_inline[0]) - 1;
	P.open_count = 0;
	memset(P.open_inline, 0, sizeof(P.open_inline));
	P.parts = P.parts_inline;
	P.parts_cap = sizeof(P.parts_inline) / sizeof(P.parts_inline[0]);
	P.buf = NULL;
	P.buf_cap = 0;

	if (len >= 3 && !memcmp(src, "\xEF\xBB\xBF", 3)) {
		P.p += 3;
	}

	array_init(rv);
	ok = parse_document(&P, Z_ARRVAL_P(rv));

	if (!ok) {
		zval_ptr_dtor(rv);
		ZVAL_UNDEF(rv);
		const char *at = MIN(P.err_at, P.end), *line = src;
		zend_long lineno = 1, col = 1;
		for (const char *q = src; (q = memchr(q, '\n', at - q)); q++) {
			lineno++;
			line = q + 1;
		}
		for (const char *q = line; q < at; q++) {
			col += ((unsigned char) *q & 0xC0) != 0x80;
		}
		err->line = lineno;
		err->column = col;
	}

	for (uint32_t i = 0; i <= P.keys_mask; i++) {
		if (P.keys[i]) {
			zend_string_release_ex(P.keys[i], 0);
		}
	}
	if (P.keys != P.keys_inline) {
		efree(P.keys);
	}
	if (P.open != P.open_inline) {
		efree(P.open);
	}
	if (P.parts != P.parts_inline) {
		efree(P.parts);
	}
	if (P.buf) {
		efree(P.buf);
	}
	return ok;
}
