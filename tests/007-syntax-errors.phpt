--TEST--
Malformed documents throw TomlParseException with a source location
--EXTENSIONS--
toml
--FILE--
<?php
$docs = [
    "key = # no value",
    "key",
    "key =",
    "= 1",
    "a = 1 b = 2",
    "a = 1\na = 2",
    "a = \"unterminated",
    "a = \"line\nbreak\"",
    "a = '''never closed",
    "a = \"bad \\q escape\"",
    "a = \"\\uD800\"",
    "a = \"\\U00110000\"",
    "a = \"\\u12\"",
    "a = [1, 2",
    "a = [1 2]",
    "a = [,]",
    "a = {x = 1",
    "a = {x = 1,, y = 2}",
    "a = {,}",
    "[table",
    "[[aot]",
    "[]",
    "[ [a]]",
    "a = 01",
    "a = 1__0",
    "a = 1_",
    "a = _1",
    "a = 1.",
    "a = .5",
    "a = 1e",
    "a = 1.2.3",
    "a = +0x10",
    "a = 0x",
    "a = 9223372036854775808",
    "a = -9223372036854775809",
    "a = 0xFFFFFFFFFFFFFFFF",
    "a = 1e400",
    "a = truee",
    "a = True",
    "a = \"\"\"a\"\"\"\"\"\"",
    "\"\"\"key\"\"\" = 1",
    "a = 1\r",
    "a = 1\rb = 2",
    "# comment \x01",
    "a = \"tab\x1f\"",
    "a.b = 1\na.b.c = 2",
    "a = 1\n[a]",
    "\n\n  x = [1,\n    2,\n    }",
];
foreach ($docs as $doc) {
    try {
        toml_parse($doc);
        echo "accepted: ", json_encode($doc), "\n";
    } catch (TomlParseException $e) {
        printf("%-34s %d:%d %s\n", json_encode($doc), $e->getTomlLine(), $e->getTomlColumn(), $e->getMessage());
    }
}
?>
--EXPECT--
"key = # no value"                 1:7 Unexpected '#', expected a value on line 1, column 7
"key"                              1:4 Unexpected end of input, expected '=' on line 1, column 4
"key ="                            1:6 Unexpected end of input, expected a value on line 1, column 6
"= 1"                              1:1 Unexpected '=', expected a key on line 1, column 1
"a = 1 b = 2"                      1:7 Unexpected 'b', expected a newline on line 1, column 7
"a = 1\na = 2"                     2:1 Duplicate key 'a' on line 2, column 1
"a = \"unterminated"               1:18 Unterminated string on line 1, column 18
"a = \"line\nbreak\""              1:10 Unterminated string on line 1, column 10
"a = '''never closed"              1:20 Unterminated multi-line string on line 1, column 20
"a = \"bad \\q escape\""           1:10 Invalid escape sequence on line 1, column 10
"a = \"\\uD800\""                  1:6 Escape \uD800 is not a Unicode scalar value on line 1, column 6
"a = \"\\U00110000\""              1:6 Escape \U00110000 is not a Unicode scalar value on line 1, column 6
"a = \"\\u12\""                    1:6 Expected 4 hex digits after \u on line 1, column 6
"a = [1, 2"                        1:10 Unexpected end of input, expected ',' or ']' on line 1, column 10
"a = [1 2]"                        1:8 Unexpected '2', expected ',' or ']' on line 1, column 8
"a = [,]"                          1:6 Unexpected ',', expected a value on line 1, column 6
"a = {x = 1"                       1:11 Unexpected end of input, expected ',' or '}' on line 1, column 11
"a = {x = 1,, y = 2}"              1:12 Unexpected ',', expected a key on line 1, column 12
"a = {,}"                          1:6 Unexpected ',', expected a key on line 1, column 6
"[table"                           1:7 Unexpected end of input, expected ']' on line 1, column 7
"[[aot]"                           1:7 Unexpected end of input, expected ']]' on line 1, column 7
"[]"                               1:2 Unexpected ']', expected a key on line 1, column 2
"[ [a]]"                           1:3 Unexpected '[', expected a key on line 1, column 3
"a = 01"                           1:5 Leading zeros are not allowed on line 1, column 5
"a = 1__0"                         1:6 Underscores must be surrounded by digits on line 1, column 6
"a = 1_"                           1:6 Underscores must be surrounded by digits on line 1, column 6
"a = _1"                           1:5 Unexpected '_', expected a value on line 1, column 5
"a = 1."                           1:7 Expected digits after the decimal point on line 1, column 7
"a = .5"                           1:5 Unexpected '.', expected a value on line 1, column 5
"a = 1e"                           1:7 Expected digits in the exponent on line 1, column 7
"a = 1.2.3"                        1:8 Unexpected '.', expected a newline on line 1, column 8
"a = +0x10"                        1:5 Hexadecimal, octal and binary integers cannot have a sign on line 1, column 5
"a = 0x"                           1:5 Expected digits after the base prefix on line 1, column 5
"a = 9223372036854775808"          1:5 Integer is out of range on line 1, column 5
"a = -9223372036854775809"         1:5 Integer is out of range on line 1, column 5
"a = 0xFFFFFFFFFFFFFFFF"           1:5 Integer is out of range on line 1, column 5
"a = 1e400"                        1:5 Float is out of range on line 1, column 5
"a = truee"                        1:9 Unexpected 'e', expected a newline on line 1, column 9
"a = True"                         1:5 Unexpected 'T', expected a value on line 1, column 5
"a = \"\"\"a\"\"\"\"\"\""          1:11 Too many quotes at the end of a multi-line string on line 1, column 11
"\"\"\"key\"\"\" = 1"              1:1 Multi-line strings cannot be used as keys on line 1, column 1
"a = 1\r"                          1:6 Unexpected byte 0x0D, expected a newline on line 1, column 6
"a = 1\rb = 2"                     1:6 Unexpected byte 0x0D, expected a newline on line 1, column 6
"# comment \u0001"                 1:11 Control character U+0001 is not allowed in comment on line 1, column 11
"a = \"tab\u001f\""                1:9 Control character U+001F is not allowed in string on line 1, column 9
"a.b = 1\na.b.c = 2"               2:3 Key 'b' already has a value on line 2, column 3
"a = 1\n[a]"                       2:2 Key 'a' already has a value on line 2, column 2
"\n\n  x = [1,\n    2,\n    }"     5:5 Unexpected '}', expected a value on line 5, column 5
