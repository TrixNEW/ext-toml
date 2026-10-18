--TEST--
Tables, dotted keys, quoted keys and arrays of tables
--EXTENSIONS--
toml
--FILE--
<?php
$r = toml_parse(<<<'TOML'
"quoted key" = 1
'literal.key' = 2
"" = "empty"
1234 = "numeric key"
007 = "not numeric"
-5 = "negative"
a.b.c = 1
a . b . d = 2
"a"."e" = 3

[fruit]
apple.color = "red"
apple.taste.sweet = true

[fruit.apple.texture]
smooth = true

[x.y.z.w]
[x]
v = 1

[[products]]
name = "Hammer"
sku = 738594937

[[products]]

[[products]]
name = "Nail"
color = "gray"

[[fruits]]
name = "apple"

[fruits.physical]
color = "red"

[[fruits.varieties]]
name = "red delicious"

[[fruits.varieties]]
name = "granny smith"

[[fruits]]
name = "banana"

[[fruits.varieties]]
name = "plantain"
TOML);
echo json_encode($r, JSON_PRETTY_PRINT), "\n";
var_dump(array_keys($r));
?>
--EXPECT--
{
    "quoted key": 1,
    "literal.key": 2,
    "": "empty",
    "1234": "numeric key",
    "007": "not numeric",
    "-5": "negative",
    "a": {
        "b": {
            "c": 1,
            "d": 2
        },
        "e": 3
    },
    "fruit": {
        "apple": {
            "color": "red",
            "taste": {
                "sweet": true
            },
            "texture": {
                "smooth": true
            }
        }
    },
    "x": {
        "y": {
            "z": {
                "w": []
            }
        },
        "v": 1
    },
    "products": [
        {
            "name": "Hammer",
            "sku": 738594937
        },
        [],
        {
            "name": "Nail",
            "color": "gray"
        }
    ],
    "fruits": [
        {
            "name": "apple",
            "physical": {
                "color": "red"
            },
            "varieties": [
                {
                    "name": "red delicious"
                },
                {
                    "name": "granny smith"
                }
            ]
        },
        {
            "name": "banana",
            "varieties": [
                {
                    "name": "plantain"
                }
            ]
        }
    ]
}
array(11) {
  [0]=>
  string(10) "quoted key"
  [1]=>
  string(11) "literal.key"
  [2]=>
  string(0) ""
  [3]=>
  int(1234)
  [4]=>
  string(3) "007"
  [5]=>
  int(-5)
  [6]=>
  string(1) "a"
  [7]=>
  string(5) "fruit"
  [8]=>
  string(1) "x"
  [9]=>
  string(8) "products"
  [10]=>
  string(6) "fruits"
}
