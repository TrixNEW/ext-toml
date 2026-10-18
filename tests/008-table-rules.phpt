--TEST--
Table definition rules
--EXTENSIONS--
toml
--FILE--
<?php
$cases = [
    'valid: header after implicit' => "[a.b.c]\n[a]\nx = 1",
    'valid: subtable of dotted' => "[fruit]\napple.color = 1\n[fruit.apple.texture]\nsmooth = true",
    'valid: aot inside dotted' => "[fruit]\napple.color = 1\n[[fruit.apple.seeds]]\nsize = 2",
    'valid: dotted under aot element' => "[[arr]]\na.b = 1\na.c = 2\n[[arr]]\na.b = 3",
    'valid: dotted extends implicit' => "[a.b.c]\n[a]\nb.d = 1",
    'redefine table' => "[a]\n[a]",
    'redefine implicit after explicit' => "[a.b]\n[a]\n[a]",
    'header over dotted' => "[t1]\nt2.t3.v = 0\n[t1.t2]",
    'header over deep dotted' => "[t1]\nt2.t3.v = 0\n[t1.t2.t3]",
    'header over root dotted' => "a.b = 1\n[a]",
    'dotted into defined' => "[a.b.c]\nz = 9\n[a]\nb.c.t = 1",
    'dotted into aot' => "[[a.b]]\n[a]\nb.y = 2",
    'dotted into scalar' => "a = 1\na.b = 2",
    'header after dotted implicit' => "[a.b.c]\n[a]\nb.d = 1\n[a.b]",
    'extend inline table by key' => "a = {k1 = 1, k1.name = 'joe'}",
    'extend inline table by header' => "a = {}\n[a.b]",
    'redefine inline table by header' => "a = {}\n[a]",
    'extend inline table by dotted' => "a = {b = 1}\na.c = 2",
    'nested inline extension' => "a = {b = {c = 1}, b.d = 2}",
    'aot over static array' => "a = []\n[[a]]",
    'header into static array' => "a = [{}]\n[a.b]",
    'table over aot' => "[[a]]\n[a]",
    'aot over table' => "[a]\n[[a]]",
    'aot over scalar' => "a = 1\n[[a]]",
    'duplicate in inline' => "a = {b = 1, b = 2}",
    'duplicate numeric key' => "1 = 'a'\n\"1\" = 'b'",
];
foreach ($cases as $name => $doc) {
    try {
        $r = toml_parse($doc);
        echo "$name: ", json_encode($r), "\n";
    } catch (TomlParseException $e) {
        echo "$name: ", $e->getMessage(), "\n";
    }
}
?>
--EXPECT--
valid: header after implicit: {"a":{"b":{"c":[]},"x":1}}
valid: subtable of dotted: {"fruit":{"apple":{"color":1,"texture":{"smooth":true}}}}
valid: aot inside dotted: {"fruit":{"apple":{"color":1,"seeds":[{"size":2}]}}}
valid: dotted under aot element: {"arr":[{"a":{"b":1,"c":2}},{"a":{"b":3}}]}
valid: dotted extends implicit: {"a":{"b":{"c":[],"d":1}}}
redefine table: Table 'a' is defined more than once on line 2, column 2
redefine implicit after explicit: Table 'a' is defined more than once on line 3, column 2
header over dotted: Table 't2' was already defined with dotted keys on line 3, column 5
header over deep dotted: Table 't3' was already defined with dotted keys on line 3, column 8
header over root dotted: Table 'a' was already defined with dotted keys on line 2, column 2
dotted into defined: Cannot add keys to table 'c' with a dotted key after its [header] on line 4, column 3
dotted into aot: Cannot add keys to array of tables 'b' with a dotted key on line 3, column 1
dotted into scalar: Key 'a' already has a value on line 2, column 1
header after dotted implicit: Table 'b' was already defined with dotted keys on line 4, column 4
extend inline table by key: Key 'k1' already has a value on line 1, column 14
extend inline table by header: Cannot extend inline table or array 'a' on line 2, column 2
redefine inline table by header: Cannot redefine inline table or array 'a' on line 2, column 2
extend inline table by dotted: Cannot extend inline table or array 'a' on line 2, column 1
nested inline extension: Cannot extend inline table or array 'b' on line 1, column 19
aot over static array: Cannot redefine 'a' as an array of tables on line 2, column 3
header into static array: Cannot extend inline table or array 'a' on line 2, column 2
table over aot: Cannot redefine array of tables 'a' as a table on line 2, column 2
aot over table: Cannot redefine 'a' as an array of tables on line 2, column 3
aot over scalar: Cannot redefine 'a' as an array of tables on line 2, column 3
duplicate in inline: Duplicate key 'b' on line 1, column 13
duplicate numeric key: Duplicate key "1" on line 2, column 1
