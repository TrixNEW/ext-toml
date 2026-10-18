--TEST--
Inline tables and arrays, including TOML 1.1 newlines and trailing commas
--EXTENSIONS--
toml
--FILE--
<?php
$r = toml_parse(<<<'TOML'
name = { first = "Tom", last = "Preston-Werner" }
point = {x=1,y=2}
animal = { type.name = "pug" }
empty = {}
multi = {
    a = 1, # comment
    b.c = [
        1,
        2, # trailing comma
    ],
    d = { e = true },
}
mixed = [ 1, "two", 3.0, [4], { five = 5 }, ]
nested = [[[]]]
aot = [ { x = 1 }, { x = 2 } ]
TOML);
echo json_encode($r), "\n";
?>
--EXPECT--
{"name":{"first":"Tom","last":"Preston-Werner"},"point":{"x":1,"y":2},"animal":{"type":{"name":"pug"}},"empty":[],"multi":{"a":1,"b":{"c":[1,2]},"d":{"e":true}},"mixed":[1,"two",3,[4],{"five":5}],"nested":[[[]]],"aot":[{"x":1},{"x":2}]}
