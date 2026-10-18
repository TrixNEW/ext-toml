<?php

// usage: php bench/run.php [--impl=ext|devium|yosymfony] [--time=1.0] [files...]
// Without files it runs every document in bench/data (run generate.php first).

$opts = getopt('', ['impl:', 'time:'], $rest);
$impl = $opts['impl'] ?? 'ext';
$budget = (float) ($opts['time'] ?? 1.0);
$files = array_slice($argv, $rest) ?: glob(__DIR__ . '/data/*.toml');

$parse = match ($impl) {
    'ext' => toml_parse(...),
    'devium' => static fn(string $s) => \Devium\Toml\Toml::decode($s, asArray: true),
    'yosymfony' => \Yosymfony\Toml\Toml::parse(...),
};
if ($impl !== 'ext') {
    require getenv('COMPOSER_AUTOLOAD') ?: __DIR__ . '/vendor/autoload.php';
}

printf("%-12s %10s %10s %10s %10s %10s %10s %10s\n", 'file', 'bytes', 'median us', 'p95 us', 'cpu us', 'MB/s', 'peak KB', 'result KB');
foreach ($files as $file) {
    $src = file_get_contents($file);
    $parse($src);

    $base = memory_get_usage();
    memory_reset_peak_usage();
    $kept = $parse($src);
    $peak = memory_get_peak_usage() - $base;
    $resident = memory_get_usage() - $base;
    unset($kept);

    // fast documents are timed in batches of at least ~20 us so the clock does not dominate
    $batch = 1;
    do {
        $t = hrtime(true);
        for ($i = 0; $i < $batch; $i++) {
            $parse($src);
        }
        $elapsed = hrtime(true) - $t;
    } while ($elapsed < 20000 && $batch *= 2);

    $samples = [];
    $deadline = hrtime(true) + (int) ($budget * 1e9);
    $ru = getrusage();
    do {
        $t = hrtime(true);
        for ($i = 0; $i < $batch; $i++) {
            $parse($src);
        }
        $samples[] = (hrtime(true) - $t) / $batch;
    } while (hrtime(true) < $deadline || count($samples) < 10);
    $ru2 = getrusage();
    $cpu = ($ru2['ru_utime.tv_sec'] - $ru['ru_utime.tv_sec']) * 1e6 + ($ru2['ru_utime.tv_usec'] - $ru['ru_utime.tv_usec'])
        + ($ru2['ru_stime.tv_sec'] - $ru['ru_stime.tv_sec']) * 1e6 + ($ru2['ru_stime.tv_usec'] - $ru['ru_stime.tv_usec']);

    sort($samples);
    $n = count($samples);
    $median = $samples[intdiv($n, 2)] / 1e3;
    $p95 = $samples[min($n - 1, (int) ($n * 0.95))] / 1e3;
    printf("%-12s %10d %10.2f %10.2f %10.2f %10.1f %10.1f %10.1f\n", basename($file, '.toml'), strlen($src), $median, $p95,
        $cpu / ($n * $batch), strlen($src) / $median, $peak / 1024, $resident / 1024);
}
