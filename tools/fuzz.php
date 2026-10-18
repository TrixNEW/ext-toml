<?php

// Mutation fuzzer. Run it under an ASan/UBSan build with USE_ZEND_ALLOC=0.
// usage: php tools/fuzz.php <corpus dir> [iterations] [seed] [--dump]
// --dump prints one line per case (accepted JSON or "reject") for differential runs.

$dir = $argv[1] ?? '';
$iterations = (int) ($argv[2] ?? 100000);
$seed = (int) ($argv[3] ?? 1);
$dump = in_array('--dump', $argv, true);

$corpus = [];
$it = new RecursiveIteratorIterator(new RecursiveDirectoryIterator($dir, FilesystemIterator::SKIP_DOTS));
foreach ($it as $file) {
    if ($file->getExtension() === 'toml' && $file->getSize() < 65536) {
        $corpus[] = file_get_contents($file->getPathname());
    }
}
if (!$corpus) {
    fwrite(STDERR, "usage: php fuzz.php <corpus dir> [iterations] [seed] [--dump]\n");
    exit(2);
}
sort($corpus);

$tokens = ['[', ']', '[[', ']]', '{', '}', '=', '.', ',', '"', "'", '"""', "'''", "\n", "\r\n", "\r", '\\', '\\u', '\\U0010FFFF',
    '#', ' ', "\t", "\0", "\x7f", "\xff", "\xc3", "\xe2\x82", "\xf0\x9f\x98\x80", '0x', '1e', '_', '-', '+', 'inf', 'nan',
    'true', '1979-05-27T07:32:00Z', '07:32', 'a.b.c', str_repeat('[', 300), str_repeat('{a=', 300)];

function mutate(string $s, array $corpus, array $tokens): string
{
    $rounds = mt_rand(1, 4);
    for ($r = 0; $r < $rounds; $r++) {
        $len = strlen($s);
        $pos = $len ? mt_rand(0, $len) : 0;
        switch (mt_rand(0, 6)) {
            case 0:
                $s = substr_replace($s, $tokens[mt_rand(0, count($tokens) - 1)], $pos, 0);
                break;
            case 1:
                $s = substr_replace($s, '', $pos, mt_rand(1, 16));
                break;
            case 2:
                if ($len) {
                    $s[min($pos, $len - 1)] = chr(mt_rand(0, 255));
                }
                break;
            case 3:
                $s = substr_replace($s, substr($s, mt_rand(0, max(0, $len - 1)), mt_rand(1, 64)), $pos, 0);
                break;
            case 4:
                $other = $corpus[mt_rand(0, count($corpus) - 1)];
                $s = substr($s, 0, $pos) . substr($other, mt_rand(0, strlen($other)));
                break;
            case 5:
                $s = substr($s, 0, $pos);
                break;
            case 6:
                $s = str_replace("\n", "\r\n", $s);
                break;
        }
    }
    return $s;
}

mt_srand($seed);
$accepted = 0;
for ($i = 0; $i < $iterations; $i++) {
    $doc = mutate($corpus[mt_rand(0, count($corpus) - 1)], $corpus, $tokens);
    try {
        $r = toml_parse($doc);
        $accepted++;
        if ($dump) {
            echo json_encode($r, JSON_PARTIAL_OUTPUT_ON_ERROR | JSON_INVALID_UTF8_SUBSTITUTE), "\n";
        }
    } catch (TomlParseException $e) {
        if ($dump) {
            echo "reject\n";
        }
    }
}
if (!$dump) {
    printf("fuzz: %d cases, %d accepted, seed %d\n", $iterations, $accepted, $seed);
}
