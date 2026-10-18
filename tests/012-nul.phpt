--TEST--
NUL bytes are rejected in the source but allowed through escapes
--EXTENSIONS--
toml
--FILE--
<?php
$r = toml_parse("\"\\u0000\" = \"a\\u0000b\"\n'\\u0000' = 1");
var_dump(array_map('bin2hex', array_keys($r)), bin2hex($r["\0"]));

foreach (["a = 1\0", "a = \"x\0y\"", "\0", "a = 1 # \0", "a\0 = 1"] as $doc) {
    try {
        toml_parse($doc);
        echo "accepted\n";
    } catch (TomlParseException $e) {
        echo $e->getMessage(), "\n";
    }
}
?>
--EXPECT--
array(2) {
  [0]=>
  string(2) "00"
  [1]=>
  string(12) "5c7530303030"
}
string(6) "610062"
Unexpected byte 0x00, expected a newline on line 1, column 6
Control character U+0000 is not allowed in string on line 1, column 7
NUL bytes are not allowed on line 1, column 1
Control character U+0000 is not allowed in comment on line 1, column 9
Unexpected byte 0x00, expected '=' on line 1, column 2
