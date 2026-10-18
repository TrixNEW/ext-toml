--TEST--
UTF-8 handling, BOM and invalid byte sequences
--EXTENSIONS--
toml
--FILE--
<?php
$r = toml_parse("\xEF\xBB\xBFname = \"Ünïcödé ✓ 😀\" # ✓ comment\n\"ключ\" = 'значение'\n'日本' = \"\"\"\n多行\n\"\"\"\n");
var_dump($r);

$bad = [
    "a = \"\xC3\x28\"",
    "a = '\xE2\x82'",
    "a = \"\xED\xA0\x80\"",
    "a = \"\xF4\x90\x80\x80\"",
    "a = \"\xC0\xAF\"",
    "a = 1 # \xFF",
    "\xC3\xA9 = 1",
    "a = 1\n\xEF\xBB\xBFb = 2",
    "a = \"✓✓\xFF\"",
];
foreach ($bad as $doc) {
    try {
        toml_parse($doc);
        echo "accepted\n";
    } catch (TomlParseException $e) {
        echo $e->getMessage(), "\n";
    }
}
?>
--EXPECT--
array(3) {
  ["name"]=>
  string(20) "Ünïcödé ✓ 😀"
  ["ключ"]=>
  string(16) "значение"
  ["日本"]=>
  string(7) "多行
"
}
Invalid UTF-8 in string on line 1, column 6
Invalid UTF-8 in string on line 1, column 6
Invalid UTF-8 in string on line 1, column 6
Invalid UTF-8 in string on line 1, column 6
Invalid UTF-8 in string on line 1, column 6
Invalid UTF-8 in comment on line 1, column 9
Unexpected byte 0xC3, expected a key on line 1, column 1
Unexpected byte 0xEF, expected a key on line 2, column 1
Invalid UTF-8 in string on line 1, column 8
