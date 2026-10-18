<?php

$out = $argv[1] ?? __DIR__ . '/data';
mt_srand(42);

function word(int $n): string
{
    static $syllables = ['qu', 'ar', 'ke', 'lo', 'mi', 'na', 'ri', 'so', 'ta', 'vel', 'xo', 'zen'];
    $w = '';
    for ($i = 0; $i < $n; $i++) {
        $w .= $syllables[mt_rand(0, count($syllables) - 1)];
    }
    return $w;
}

$large = "# generated catalogue\n\ntitle = \"Item catalogue\"\nversion = 3\ngenerated = 2026-10-18T12:00:00.250+02:00\n\n";
$large .= "[lang]\n";
for ($i = 0; $i < 4000; $i++) {
    $large .= sprintf("\"%s.%s.%d\" = \"%s %s %s\"\n", word(2), word(3), $i, ucfirst(word(3)), word(2), word(4));
}
for ($i = 0; $i < 3000; $i++) {
    $large .= "\n[[items]]\n";
    $large .= sprintf("id = %d\n", 100000 + $i);
    $large .= sprintf("name = \"%s\"\n", ucfirst(word(3)) . ' ' . word(2));
    $large .= sprintf("description = \"%s \\\"%s\\\" %s\\n%s\"\n", word(4), word(2), word(5), word(3));
    $large .= sprintf("price = %d.%02d\n", mt_rand(1, 9999), mt_rand(0, 99));
    $large .= sprintf("stock = %d\n", mt_rand(0, 100000));
    $large .= sprintf("weight = %se-%d\n", mt_rand(1, 999) / 10, mt_rand(1, 5));
    $large .= 'enabled = ' . (mt_rand(0, 1) ? 'true' : 'false') . "\n";
    $large .= sprintf("added = 20%02d-%02d-%02d\n", mt_rand(10, 26), mt_rand(1, 12), mt_rand(1, 28));
    $large .= sprintf("tags = [\"%s\", \"%s\", \"%s\"]\n", word(2), word(2), word(3));
    $large .= sprintf("dims = { w = %d, h = %d, d = %d }\n", mt_rand(1, 500), mt_rand(1, 500), mt_rand(1, 500));
    $large .= sprintf("vendor.name = \"%s\"\nvendor.code = 0x%X\n", ucfirst(word(3)), mt_rand(0, 0xFFFFFF));
    $large .= sprintf("notes = '''\n%s\n%s'''\n", word(6), word(4));
}
file_put_contents("$out/large.toml", $large);

$deep = "# generated nesting\n\n";
for ($i = 0; $i < 200; $i++) {
    $path = [];
    for ($d = 0; $d < 8; $d++) {
        $path[] = 'n' . ($i % (3 + $d)) . '_' . $d;
    }
    $deep .= '[' . implode('.', $path) . "]\nleaf$i = $i\nname = \"" . word(2) . "\"\n\n";
}
$arr = '1';
$tab = '1';
for ($d = 0; $d < 24; $d++) {
    $arr = "[$arr, $d]";
    $tab = "{ v = $tab, d = $d }";
}
for ($i = 0; $i < 50; $i++) {
    $deep .= "[inline.k$i]\narr = $arr\ntab = $tab\n\n";
}
file_put_contents("$out/deep.toml", $deep);

foreach (['large', 'deep'] as $f) {
    printf("%s.toml: %d bytes\n", $f, filesize("$out/$f.toml"));
}
