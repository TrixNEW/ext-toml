#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "ext/standard/info.h"
#include "zend_exceptions.h"
#include "php_toml.h"
#include "src/parser.h"
#include "toml_arginfo.h"

static zend_class_entry *toml_exception_ce;
static zend_class_entry *toml_parse_exception_ce;

static void throw_parse_error(const php_toml_error *err, const zend_string *path)
{
	zend_object *ex;
	if (path) {
		ex = zend_throw_exception_ex(toml_parse_exception_ce, 0, "%s in %s on line " ZEND_LONG_FMT ", column " ZEND_LONG_FMT,
			err->message, ZSTR_VAL(path), err->line, err->column);
	} else {
		ex = zend_throw_exception_ex(toml_parse_exception_ce, 0, "%s on line " ZEND_LONG_FMT ", column " ZEND_LONG_FMT,
			err->message, err->line, err->column);
	}
	zend_update_property_long(toml_parse_exception_ce, ex, ZEND_STRL("tomlLine"), err->line);
	zend_update_property_long(toml_parse_exception_ce, ex, ZEND_STRL("tomlColumn"), err->column);
}

PHP_FUNCTION(toml_parse)
{
	zend_string *source;
	php_toml_error err;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_STR(source)
	ZEND_PARSE_PARAMETERS_END();

	if (!php_toml_decode(return_value, ZSTR_VAL(source), ZSTR_LEN(source), &err)) {
		throw_parse_error(&err, NULL);
	}
}

PHP_FUNCTION(toml_parse_file)
{
	zend_string *path, *contents = NULL;
	zend_error_handling eh;
	php_stream_statbuf ssb;
	php_toml_error err;

	ZEND_PARSE_PARAMETERS_START(1, 1)
		Z_PARAM_PATH_STR(path)
	ZEND_PARSE_PARAMETERS_END();

	if (ZSTR_LEN(path) == 0) {
		zend_argument_must_not_be_empty_error(1);
		RETURN_THROWS();
	}

	/* stream warnings become TomlException */
	zend_replace_error_handling(EH_THROW, toml_exception_ce, &eh);
	php_stream *stream = php_stream_open_wrapper_ex(ZSTR_VAL(path), "rb", REPORT_ERRORS, NULL, NULL);
	if (stream) {
		if (php_stream_stat(stream, &ssb) == 0 && S_ISDIR(ssb.sb.st_mode)) {
			zend_throw_exception_ex(toml_exception_ce, 0, "%s is a directory", ZSTR_VAL(path));
		} else {
			contents = php_stream_copy_to_mem(stream, PHP_STREAM_COPY_ALL, 0);
		}
		php_stream_close(stream);
	}
	zend_restore_error_handling(&eh);

	if (EG(exception)) {
		if (contents) {
			zend_string_release_ex(contents, 0);
		}
		RETURN_THROWS();
	}
	if (!contents) {
		RETURN_EMPTY_ARRAY();
	}
	if (!php_toml_decode(return_value, ZSTR_VAL(contents), ZSTR_LEN(contents), &err)) {
		throw_parse_error(&err, path);
	}
	zend_string_release_ex(contents, 0);
}

PHP_METHOD(TomlParseException, getTomlLine)
{
	zval rv;
	ZEND_PARSE_PARAMETERS_NONE();
	RETURN_COPY_DEREF(zend_read_property(toml_parse_exception_ce, Z_OBJ_P(ZEND_THIS), ZEND_STRL("tomlLine"), 0, &rv));
}

PHP_METHOD(TomlParseException, getTomlColumn)
{
	zval rv;
	ZEND_PARSE_PARAMETERS_NONE();
	RETURN_COPY_DEREF(zend_read_property(toml_parse_exception_ce, Z_OBJ_P(ZEND_THIS), ZEND_STRL("tomlColumn"), 0, &rv));
}

static PHP_MINIT_FUNCTION(toml)
{
	toml_exception_ce = register_class_TomlException(zend_ce_exception);
	toml_parse_exception_ce = register_class_TomlParseException(toml_exception_ce);
	return SUCCESS;
}

static PHP_RINIT_FUNCTION(toml)
{
#if defined(ZTS) && defined(COMPILE_DL_TOML)
	ZEND_TSRMLS_CACHE_UPDATE();
#endif
	return SUCCESS;
}

static PHP_MINFO_FUNCTION(toml)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "toml support", "enabled");
	php_info_print_table_row(2, "Version", PHP_TOML_VERSION);
	php_info_print_table_row(2, "TOML version", "1.1.0");
	php_info_print_table_end();
}

zend_module_entry toml_module_entry = {
	STANDARD_MODULE_HEADER,
	"toml",
	ext_functions,
	PHP_MINIT(toml),
	NULL,
	PHP_RINIT(toml),
	NULL,
	PHP_MINFO(toml),
	PHP_TOML_VERSION,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_TOML
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(toml)
#endif
