<?php

// Runs the toml-test corpus (github.com/toml-lang/toml-test) against the extension.
// usage: php tools/toml-test.php <toml-test/tests> [1.0.0|1.1.0] [-v]

$dir = rtrim($argv[1] ?? '', '/\\');
$version = $argv[2] ?? '1.1.0';
$verbose = in_array('-v', $argv, true);
if (!is_file("$dir/files-toml-$version")) {
    fwrite(STDERR, "usage: php toml-test.php <toml-test/tests> [1.0.0|1.1.0] [-v]\n");
    exit(2);
}

function normalize_datetime(string $s): string
{
    $s = strtoupper(str_replace(' ', 'T', $s));
    $s = preg_replace('/(\.\d*?)0+(?=$|Z|[+-]\d\d:\d\d$)/', '$1', $s);
    $s = preg_replace('/\.(?=$|Z|[+-])/', '', $s);
    return preg_replace('/[+-]00:00$/', 'Z', $s);
}

function same($expected, $actual, string $path): ?string
{
    if (count($expected) === 2 && is_string($expected['type'] ?? null) && is_string($expected['value'] ?? null)) {
        return same_scalar($expected['type'], $expected['value'], $actual, $path);
    }
    if (!is_array($actual)) {
        return "$path: expected table or array, got " . get_debug_type($actual);
    }
    if (array_is_list($expected) !== array_is_list($actual)) {
        return "$path: expected " . (array_is_list($expected) ? 'array' : 'table');
    }
    if (count($expected) !== count($actual)) {
        return "$path: expected " . count($expected) . ' entries, got ' . count($actual);
    }
    foreach ($expected as $k => $v) {
        if (!array_key_exists($k, $actual)) {
            return "$path: missing key " . json_encode((string) $k);
        }
        if ($err = same($v, $actual[$k], "$path.$k")) {
            return $err;
        }
    }
    return null;
}

function same_scalar(string $type, string $value, $actual, string $path): ?string
{
    $ok = match ($type) {
        'string' => $actual === $value,
        'integer' => is_int($actual) && (string) $actual === $value,
        'float' => is_float($actual) && match (true) {
            str_ends_with($value, 'nan') => is_nan($actual),
            str_ends_with($value, 'inf') => $actual === ($value[0] === '-' ? -INF : INF),
            default => $actual === (float) $value && (fdiv(1, $actual) < 0) === (fdiv(1, (float) $value) < 0),
        },
        'bool' => $actual === ($value === 'true'),
        'datetime', 'datetime-local', 'date-local', 'time-local' => is_string($actual) && normalize_datetime($actual) === normalize_datetime($value),
        default => false,
    };
    return $ok ? null : "$path: expected $type " . json_encode($value) . ', got ' . var_export($actual, true);
}

$pass = $fail = 0;
foreach (file("$dir/files-toml-$version", FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES) as $file) {
    if (!str_ends_with($file, '.toml')) {
        continue;
    }
    $valid = str_starts_with($file, 'valid/');
    $src = file_get_contents("$dir/$file");
    $err = null;
    try {
        $result = toml_parse($src);
        if ($valid) {
            $err = same(json_decode(file_get_contents("$dir/" . substr($file, 0, -5) . '.json'), true, 512, JSON_THROW_ON_ERROR), $result, '');
        } else {
            $err = 'accepted invalid document';
        }
    } catch (TomlParseException $e) {
        if ($valid) {
            $err = 'rejected: ' . $e->getMessage();
        }
    } catch (Throwable $e) {
        $err = get_class($e) . ': ' . $e->getMessage();
    }
    if ($err === null) {
        $pass++;
        continue;
    }
    $fail++;
    if ($verbose) {
        echo "FAIL $file\n  $err\n";
    }
}

printf("toml-test %s: %d passed, %d failed\n", $version, $pass, $fail);
exit($fail ? 1 : 0);
