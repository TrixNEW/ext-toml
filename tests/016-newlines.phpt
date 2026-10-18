--TEST--
CRLF line endings
--EXTENSIONS--
toml
--FILE--
<?php
$r = toml_parse("a = 1\r\n# c\r\n[t]\r\nb = \"\"\"\r\nx\r\ny\"\"\"\r\nc = [\r\n 1,\r\n]\r\nd = '''\r\nraw\r\n'''\r\ne = \"\"\"a \\\r\n   b\"\"\"\r\n");
echo json_encode($r), "\n";
?>
--EXPECT--
{"a":1,"t":{"b":"x\r\ny","c":[1],"d":"raw\r\n","e":"a b"}}
