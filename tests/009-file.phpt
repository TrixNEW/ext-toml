--TEST--
toml_parse_file() reads through PHP streams
--EXTENSIONS--
toml
--FILE--
<?php
$dir = __DIR__ . '/009-file';
@mkdir($dir);
file_put_contents("$dir/ok.toml", "a = 1\n[b]\nc = 'x'\n");
file_put_contents("$dir/bad.toml", "a = 1\nb = \n");
file_put_contents("$dir/empty.toml", "");

var_dump(toml_parse_file("$dir/ok.toml"));
var_dump(toml_parse_file("$dir/empty.toml"));
var_dump(toml_parse_file("data://text/plain;base64," . base64_encode("x = true")));

try {
    toml_parse_file("$dir/bad.toml");
} catch (TomlParseException $e) {
    echo str_replace($dir, 'DIR', $e->getMessage()), "\n";
}
foreach (["$dir/missing.toml", $dir] as $path) {
    try {
        toml_parse_file($path);
    } catch (TomlException $e) {
        echo get_class($e), ': ', str_replace($dir, 'DIR', $e->getMessage()), "\n";
    }
}
foreach (["", "a\0b"] as $path) {
    try {
        toml_parse_file($path);
    } catch (ValueError $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}
?>
--CLEAN--
<?php
$dir = __DIR__ . '/009-file';
foreach (['ok', 'bad', 'empty'] as $f) {
    @unlink("$dir/$f.toml");
}
@rmdir($dir);
?>
--EXPECTF--
array(2) {
  ["a"]=>
  int(1)
  ["b"]=>
  array(1) {
    ["c"]=>
    string(1) "x"
  }
}
array(0) {
}
array(1) {
  ["x"]=>
  bool(true)
}
Unexpected newline, expected a value in DIR%cbad.toml on line 2, column 5
TomlException: toml_parse_file(DIR%cmissing.toml): Failed to open stream: No such file or directory
TomlException: DIR is a directory
ValueError: toml_parse_file(): Argument #1 ($path) must not be empty
ValueError: toml_parse_file(): Argument #1 ($path) must not contain any null bytes
