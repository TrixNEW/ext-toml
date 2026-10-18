/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: c3d631a49e894564b7c9eb18d21bf01107bde709 */

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_toml_parse, 0, 1, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, source, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_toml_parse_file, 0, 1, IS_ARRAY, 0)
	ZEND_ARG_TYPE_INFO(0, path, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_TomlParseException_getTomlLine, 0, 0, IS_LONG, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_TomlParseException_getTomlColumn arginfo_class_TomlParseException_getTomlLine

ZEND_FUNCTION(toml_parse);
ZEND_FUNCTION(toml_parse_file);
ZEND_METHOD(TomlParseException, getTomlLine);
ZEND_METHOD(TomlParseException, getTomlColumn);

static const zend_function_entry ext_functions[] = {
	ZEND_FE(toml_parse, arginfo_toml_parse)
	ZEND_FE(toml_parse_file, arginfo_toml_parse_file)
	ZEND_FE_END
};

static const zend_function_entry class_TomlParseException_methods[] = {
	ZEND_ME(TomlParseException, getTomlLine, arginfo_class_TomlParseException_getTomlLine, ZEND_ACC_PUBLIC)
	ZEND_ME(TomlParseException, getTomlColumn, arginfo_class_TomlParseException_getTomlColumn, ZEND_ACC_PUBLIC)
	ZEND_FE_END
};

static zend_class_entry *register_class_TomlException(zend_class_entry *class_entry_Exception)
{
	zend_class_entry ce, *class_entry;

	INIT_CLASS_ENTRY(ce, "TomlException", NULL);
	class_entry = zend_register_internal_class_with_flags(&ce, class_entry_Exception, 0);

	return class_entry;
}

static zend_class_entry *register_class_TomlParseException(zend_class_entry *class_entry_TomlException)
{
	zend_class_entry ce, *class_entry;

	INIT_CLASS_ENTRY(ce, "TomlParseException", class_TomlParseException_methods);
	class_entry = zend_register_internal_class_with_flags(&ce, class_entry_TomlException, ZEND_ACC_FINAL);

	zval property_tomlLine_default_value;
	ZVAL_LONG(&property_tomlLine_default_value, 0);
	zend_string *property_tomlLine_name = zend_string_init("tomlLine", sizeof("tomlLine") - 1, 1);
	zend_declare_typed_property(class_entry, property_tomlLine_name, &property_tomlLine_default_value, ZEND_ACC_PRIVATE, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(property_tomlLine_name);

	zval property_tomlColumn_default_value;
	ZVAL_LONG(&property_tomlColumn_default_value, 0);
	zend_string *property_tomlColumn_name = zend_string_init("tomlColumn", sizeof("tomlColumn") - 1, 1);
	zend_declare_typed_property(class_entry, property_tomlColumn_name, &property_tomlColumn_default_value, ZEND_ACC_PRIVATE, NULL, (zend_type) ZEND_TYPE_INIT_MASK(MAY_BE_LONG));
	zend_string_release(property_tomlColumn_name);

	return class_entry;
}
