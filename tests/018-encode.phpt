--TEST--
toml_encode() writes documents toml_parse() reads back
--EXTENSIONS--
toml
--FILE--
<?php
echo toml_encode([
    'title' => 'demo',
    'port' => 19132,
    'ratio' => 0.5,
    'whole' => 2.0,
    'on' => false,
    'list' => [1, 'two', [3]],
    'inline' => [['a' => 1], 2],
    'empty' => [],
    'a.b' => 'quoted key',
    '' => 'empty key',
    'server' => ['motd' => "Line\n\"q\" \\ \t\x01\x7f", 'limits' => ['max' => 20]],
    'kits' => [['name' => 'starter'], ['name' => 'vip', 'meta' => ['tier' => 2]]],
]);
echo "--\n";
var_dump(toml_encode([]));
echo toml_encode(['one', 'two']);
echo toml_encode(['outer' => ['inner' => ['x' => 1]]]);
echo toml_encode(['f' => [0.1, -0.0, 1e300, 1.5e-7, INF, -INF, NAN], 'n' => PHP_INT_MIN, 'r' => [1 => 'a', -5 => 'b']]);

$data = [
    'ключ' => 'unicode ✓ 😀',
    'ranks' => [1 => 'guest', 5 => 'admin'],
    'slots' => [0 => 'a', 2 => 'c'],
    'with space' => ['x.y' => true],
    'mixed' => [1, 'a', ['k' => 'v'], [2]],
    'deep' => ['a' => ['b' => ['c' => ['d' => 'e']]]],
    'floats' => [0.1, 1e300, -2.5e-300, 123456789.125],
];
var_dump(toml_parse(toml_encode($data)) == $data);

$ref = ['v' => 1];
$alias = &$ref;
var_dump(toml_encode(['r' => &$ref, 'l' => [&$alias]]) === "[r]\nv = 1\n\n[[l]]\nv = 1\n");

$nested = 'x';
for ($i = 0; $i < 255; $i++) {
    $nested = [$nested];
}
var_dump(toml_parse(toml_encode(['k' => $nested]))['k'] === $nested);
$tables = 'x';
for ($i = 0; $i < 256; $i++) {
    $tables = ['t' => $tables];
}
var_dump(toml_parse(toml_encode($tables)) === $tables);

$cycle = [];
$cycle['self'] = &$cycle;
$list = [];
$list[] = &$list;
foreach ([
    ['k' => null],
    ['k' => new stdClass()],
    ['k' => STDIN],
    ['k' => "\xff"],
    ["\xc3" => 1],
    ['k' => "\xed\xa0\x80"],
    ['k' => [$nested]],
    [$tables],
    $cycle,
    ['k' => $list],
] as $bad) {
    try {
        toml_encode($bad);
        echo "encoded\n";
    } catch (TomlException $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}

try {
    toml_encode('x');
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
title = "demo"
port = 19132
ratio = 0.5
whole = 2.0
on = false
list = [1, "two", [3]]
inline = [{ a = 1 }, 2]
empty = []
"a.b" = "quoted key"
"" = "empty key"

[server]
motd = "Line\n\"q\" \\ \t\u0001\u007F"

[server.limits]
max = 20

[[kits]]
name = "starter"

[[kits]]
name = "vip"

[kits.meta]
tier = 2
--
string(0) ""
0 = "one"
1 = "two"
[outer]

[outer.inner]
x = 1
f = [0.1, -0.0, 1.0E+300, 1.5E-7, inf, -inf, nan]
n = -9223372036854775808

[r]
1 = "a"
-5 = "b"
bool(true)
bool(true)
bool(true)
bool(true)
TomlException: Cannot encode null to TOML
TomlException: Cannot encode stdClass to TOML
TomlException: Cannot encode resource to TOML
TomlException: Invalid UTF-8 in string
TomlException: Invalid UTF-8 in key
TomlException: Invalid UTF-8 in string
TomlException: Nesting exceeds 256 levels
TomlException: Nesting exceeds 256 levels
TomlException: Nesting exceeds 256 levels
TomlException: Nesting exceeds 256 levels
toml_encode(): Argument #1 ($data) must be of type array, string given
