--TEST--
Nesting limit and large documents
--EXTENSIONS--
toml
--INI--
memory_limit=512M
--FILE--
<?php
$ok = str_repeat('[', 256) . str_repeat(']', 256);
$r = toml_parse("a = $ok");
$depth = 0;
for ($v = $r['a']; is_array($v); $v = $v[0] ?? null) {
    $depth++;
}
var_dump($depth);

foreach (["a = " . str_repeat('[', 257) . str_repeat(']', 257), "a = " . str_repeat('{b=', 257) . '1' . str_repeat('}', 257), "a = " . str_repeat('[', 100000)] as $doc) {
    try {
        toml_parse($doc);
    } catch (TomlParseException $e) {
        echo $e->getMessage(), "\n";
    }
}

$n = 200000;
$r = toml_parse('a = [' . implode(',', range(1, $n)) . ']');
var_dump(count($r['a']), $r['a'][$n - 1]);

$doc = '';
for ($i = 0; $i < 50000; $i++) {
    $doc .= "k$i = $i\n";
}
$r = toml_parse($doc);
var_dump(count($r), $r['k49999']);

$key = implode('.', array_fill(0, 1000, 'k'));
$r = toml_parse("$key = 1\n[" . implode('.', array_fill(0, 1000, 'h')) . "]\n");
$v = $r;
$parts = 0;
while (is_array($v) && isset($v['k'])) {
    $v = $v['k'];
    $parts++;
}
var_dump($parts, $v);

$s = str_repeat('x', 5_000_000);
var_dump(strlen(toml_parse("s = \"$s\"")['s']), strlen(toml_parse("s = '''$s'''")['s']));
?>
--EXPECT--
int(256)
Nesting is deeper than 256 levels on line 1, column 261
Nesting is deeper than 256 levels on line 1, column 773
Nesting is deeper than 256 levels on line 1, column 261
int(200000)
int(200000)
int(50000)
int(49999)
int(1000)
int(1)
int(5000000)
int(5000000)
