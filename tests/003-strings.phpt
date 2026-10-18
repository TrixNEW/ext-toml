--TEST--
Basic, literal and multi-line strings with escapes
--EXTENSIONS--
toml
--FILE--
<?php
$r = toml_parse(<<<'TOML'
basic = "I'm a string. \"You can quote me\". Name\tJos\u00E9\nLocation\tSF."
bytes = "caf\xE9 \e[0m"
big = "\U0001F600"
empty = ""
one = "\n"
lit = 'C:\Users\nodejs\templates'
litq = '<\i\c*\s*>'
ml = """
Roses are red
Violets are blue"""
ml2 = """\
       The quick brown \
       fox jumps over \
       the lazy dog.\
       """
ml3 = """Here are two quotation marks: "". Simple enough."""
ml4 = """Here are fifteen quotation marks: ""\"""\"""\"""\"""\"."""
ml5 = """"This," she said, "is just a pointless statement.""""
ml6 = """trailing backslash with spaces \
  continues"""
mll = '''
The first newline is
trimmed in raw strings.
   All other whitespace
   is preserved.
'''
mll2 = ''''That,' she said, 'is still pointless.''''
tab = "a	b"
TOML);
foreach ($r as $k => $v) {
    echo $k, ': ', json_encode($v, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES), "\n";
}
var_dump(strlen($r['big']), bin2hex($r['bytes']));
?>
--EXPECT--
basic: "I'm a string. \"You can quote me\". Name\tJosé\nLocation\tSF."
bytes: "café \u001b[0m"
big: "😀"
empty: ""
one: "\n"
lit: "C:\\Users\\nodejs\\templates"
litq: "<\\i\\c*\\s*>"
ml: "Roses are red\nViolets are blue"
ml2: "The quick brown fox jumps over the lazy dog."
ml3: "Here are two quotation marks: \"\". Simple enough."
ml4: "Here are fifteen quotation marks: \"\"\"\"\"\"\"\"\"\"\"\"\"\"\"."
ml5: "\"This,\" she said, \"is just a pointless statement.\""
ml6: "trailing backslash with spaces continues"
mll: "The first newline is\ntrimmed in raw strings.\n   All other whitespace\n   is preserved.\n"
mll2: "'That,' she said, 'is still pointless.'"
tab: "a\tb"
int(4)
string(20) "636166c3a9201b5b306d"
