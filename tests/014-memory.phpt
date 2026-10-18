--TEST--
Repeated parsing does not grow memory and results are independent values
--EXTENSIONS--
toml
--FILE--
<?php
$doc = file_get_contents(__DIR__ . '/../bench/data/typical.toml');
$bad = $doc . "\n[server]\n";

toml_parse($doc);
try { toml_parse($bad); } catch (TomlParseException) {}
$base = memory_get_usage();
for ($i = 0; $i < 2000; $i++) {
    $r = toml_parse($doc);
    unset($r);
    try {
        toml_parse($bad);
    } catch (TomlParseException $e) {
        unset($e);
    }
}
var_dump(memory_get_usage() - $base);

$a = toml_parse($doc);
$b = toml_parse($doc);
$a['plugins'][0]['name'] = 'changed';
$a['server']['new'] = 1;
unset($a['database']);
var_dump($b['plugins'][0]['name'], isset($b['server']['new']), isset($b['database']));
var_dump($a == toml_parse($doc), $b == toml_parse($doc));
?>
--EXPECT--
int(0)
string(10) "Essentials"
bool(false)
bool(true)
bool(false)
bool(true)
