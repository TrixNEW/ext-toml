--TEST--
toml_parse_file() respects open_basedir
--EXTENSIONS--
toml
--INI--
open_basedir={PWD}
--FILE--
<?php
try {
    toml_parse_file(dirname(__DIR__) . '/config.m4');
} catch (TomlException $e) {
    echo get_class($e), "\n";
    var_dump(str_contains($e->getMessage(), 'open_basedir restriction in effect'));
}
?>
--EXPECT--
TomlException
bool(true)
