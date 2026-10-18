--TEST--
Integers, floats and booleans
--EXTENSIONS--
toml
--SKIPIF--
<?php if (PHP_INT_SIZE !== 8) die('skip 64-bit only'); ?>
--FILE--
<?php
$r = toml_parse(<<<'TOML'
i1 = +99
i2 = 42
i3 = 0
i4 = -17
i5 = 1_000_000
i6 = 5_349_221
i7 = -0
max = 9223372036854775807
min = -9223372036854775808
hex = 0xDEAD_beef
oct = 0o755
bin = 0b1101_0110
hexmax = 0x7FFFFFFFFFFFFFFF
f1 = +1.0
f2 = 3.1415
f3 = -0.01
f4 = 5e+22
f5 = 1e06
f6 = -2E-2
f7 = 6.626e-34
f8 = 224_617.445_991_228
f9 = -0.0
f10 = 0e0
f11 = 1e-400
pinf = inf
ninf = -inf
pinf2 = +inf
nan1 = nan
nan2 = +nan
nan3 = -nan
t = true
f = false
TOML);
foreach ($r as $k => $v) {
    echo $k, ': ', var_export($v, true), "\n";
}
var_dump(is_nan($r['nan3']), fdiv(1, $r['f9']));
?>
--EXPECT--
i1: 99
i2: 42
i3: 0
i4: -17
i5: 1000000
i6: 5349221
i7: 0
max: 9223372036854775807
min: -9223372036854775807-1
hex: 3735928559
oct: 493
bin: 214
hexmax: 9223372036854775807
f1: 1.0
f2: 3.1415
f3: -0.01
f4: 5.0E+22
f5: 1000000.0
f6: -0.02
f7: 6.626E-34
f8: 224617.445991228
f9: -0.0
f10: 0.0
f11: 0.0
pinf: INF
ninf: -INF
pinf2: INF
nan1: NAN
nan2: NAN
nan3: NAN
t: true
f: false
bool(true)
float(-INF)
