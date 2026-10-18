--TEST--
Exception classes and argument handling
--EXTENSIONS--
toml
--FILE--
<?php
var_dump(is_subclass_of('TomlException', 'Exception'));
var_dump(is_subclass_of('TomlParseException', 'TomlException'));
var_dump((new ReflectionClass('TomlParseException'))->isFinal());

try {
    toml_parse("\n\nfoo = bar\n");
} catch (TomlException $e) {
    var_dump(get_class($e), $e->getTomlLine(), $e->getTomlColumn(), $e->getCode(), $e->getPrevious());
}

$e = new TomlParseException('manual');
var_dump($e->getTomlLine(), $e->getTomlColumn());

try {
    toml_parse([]);
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
string(18) "TomlParseException"
int(3)
int(7)
int(0)
NULL
int(0)
int(0)
toml_parse(): Argument #1 ($source) must be of type string, array given
