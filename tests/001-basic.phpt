--TEST--
Basic document
--EXTENSIONS--
toml
--FILE--
<?php
var_dump(toml_parse(<<<'TOML'
# comment
title = "TOML Example"

[owner]
name = "Tom Preston-Werner"
dob = 1979-05-27T07:32:00-08:00

[database]
enabled = true
ports = [ 8000, 8001, 8002 ]
data = [ ["delta", "phi"], [3.14] ]
temp_targets = { cpu = 79.5, case = 72.0 }
TOML));
var_dump(toml_parse(''));
var_dump(toml_parse("\n\n# only comments\n   \t\n"));
?>
--EXPECT--
array(3) {
  ["title"]=>
  string(12) "TOML Example"
  ["owner"]=>
  array(2) {
    ["name"]=>
    string(18) "Tom Preston-Werner"
    ["dob"]=>
    string(25) "1979-05-27T07:32:00-08:00"
  }
  ["database"]=>
  array(4) {
    ["enabled"]=>
    bool(true)
    ["ports"]=>
    array(3) {
      [0]=>
      int(8000)
      [1]=>
      int(8001)
      [2]=>
      int(8002)
    }
    ["data"]=>
    array(2) {
      [0]=>
      array(2) {
        [0]=>
        string(5) "delta"
        [1]=>
        string(3) "phi"
      }
      [1]=>
      array(1) {
        [0]=>
        float(3.14)
      }
    }
    ["temp_targets"]=>
    array(2) {
      ["cpu"]=>
      float(79.5)
      ["case"]=>
      float(72)
    }
  }
}
array(0) {
}
array(0) {
}
