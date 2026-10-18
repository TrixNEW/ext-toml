--TEST--
Concurrent parsing from several threads (ZTS with pmmpthread)
--EXTENSIONS--
toml
--SKIPIF--
<?php
if (!PHP_ZTS || !class_exists('pmmp\thread\Thread')) die('skip needs ZTS and pmmpthread');
?>
--FILE--
<?php
use pmmp\thread\Thread;

final class ParseThread extends Thread
{
    public int $mismatches = 0;
    public int $errors = 0;

    public function __construct(private string $doc, private string $file, private string $expected, private int $rounds)
    {
    }

    public function run(): void
    {
        for ($i = 0; $i < $this->rounds; $i++) {
            if (md5(serialize(toml_parse($this->doc))) !== $this->expected) {
                $this->mismatches++;
            }
            if (md5(serialize(toml_parse_file($this->file))) !== $this->expected) {
                $this->mismatches++;
            }
            try {
                toml_parse($this->doc . "\n[server]\n");
            } catch (TomlParseException $e) {
                $this->errors++;
            }
        }
    }
}

$file = __DIR__ . '/../bench/data/typical.toml';
$doc = file_get_contents($file);
$expected = md5(serialize(toml_parse($doc)));
$rounds = 300;

$threads = [];
for ($t = 0; $t < 8; $t++) {
    $threads[] = $thread = new ParseThread($doc, $file, $expected, $rounds);
    $thread->start(Thread::INHERIT_ALL);
}
$mismatches = $errors = 0;
foreach ($threads as $thread) {
    $thread->join();
    $mismatches += $thread->mismatches;
    $errors += $thread->errors;
}
var_dump($mismatches, $errors === 8 * $rounds);
?>
--EXPECT--
int(0)
bool(true)
