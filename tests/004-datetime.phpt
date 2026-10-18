--TEST--
Date and time values become normalized RFC 3339 strings
--EXTENSIONS--
toml
--FILE--
<?php
$r = toml_parse(<<<'TOML'
odt1 = 1979-05-27T07:32:00Z
odt2 = 1979-05-27T00:32:00-07:00
odt3 = 1979-05-27T00:32:00.999999-07:00
odt4 = 1979-05-27 07:32:00z
odt5 = 1979-05-27t07:32:00.123456789+05:30
odt6 = 1979-05-27T07:32Z
ldt1 = 1979-05-27T07:32:00
ldt2 = 1979-05-27T00:32:00.5
ldt3 = 1979-05-27 07:32
ld = 1979-05-27
lt1 = 07:32:00
lt2 = 00:32:00.999999
lt3 = 07:32
leap = 2024-02-29
leapsec = 1990-12-31T23:59:60Z
arr = [1979-05-27, 07:32:00, 1979-05-27T07:32:00Z]
TOML);
foreach ($r as $k => $v) {
    echo $k, ': ', json_encode($v), "\n";
}
foreach (['bad = 2023-02-29', 'bad = 1979-13-01', 'bad = 1979-05-27T24:00:00', 'bad = 07:60', 'bad = 1979-05-27T07:32:00+24:00', 'bad = 1979-5-27', 'bad = 07:32:00Z', 'bad = 1979-05-27T07:32:00.'] as $doc) {
    try {
        toml_parse($doc);
        echo "accepted: $doc\n";
    } catch (TomlParseException $e) {
        echo $e->getMessage(), "\n";
    }
}
?>
--EXPECT--
odt1: "1979-05-27T07:32:00Z"
odt2: "1979-05-27T00:32:00-07:00"
odt3: "1979-05-27T00:32:00.999999-07:00"
odt4: "1979-05-27T07:32:00Z"
odt5: "1979-05-27T07:32:00.123456789+05:30"
odt6: "1979-05-27T07:32:00Z"
ldt1: "1979-05-27T07:32:00"
ldt2: "1979-05-27T00:32:00.5"
ldt3: "1979-05-27T07:32:00"
ld: "1979-05-27"
lt1: "07:32:00"
lt2: "00:32:00.999999"
lt3: "07:32:00"
leap: "2024-02-29"
leapsec: "1990-12-31T23:59:60Z"
arr: ["1979-05-27","07:32:00","1979-05-27T07:32:00Z"]
Invalid date 2023-02-29 on line 1, column 7
Invalid date 1979-13-01 on line 1, column 7
Invalid time 24:00:00 on line 1, column 18
Invalid time 07:60 on line 1, column 7
Invalid time offset +24:00 on line 1, column 26
Invalid date, expected YYYY-MM-DD on line 1, column 7
Unexpected 'Z', expected a newline on line 1, column 15
Expected digits after the decimal point on line 1, column 27
