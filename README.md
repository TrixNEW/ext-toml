# ext-toml

A fast, strict [TOML 1.1](https://toml.io/en/v1.1.0) parser for PHP 8.4+, written in C17.

- Passes all 712 tests of the official [toml-test](https://github.com/toml-lang/toml-test) suite for TOML 1.1.0.
- Builds PHP arrays directly while parsing, with no intermediate tree.
- ZTS and NTS, Linux, macOS and Windows, shared or compiled into PHP.
- No dependencies and no global mutable state. All memory goes through the Zend allocator, so `memory_limit` applies.

## API

```php
function toml_parse(string $source): array;
function toml_parse_file(string $path): array;

class TomlException extends Exception {}

final class TomlParseException extends TomlException
{
    public function getTomlLine(): int {}
    public function getTomlColumn(): int {}
}
```

```php
$config = toml_parse_file(__DIR__ . '/server.toml');

try {
    toml_parse("port = 19132\nport = 19133");
} catch (TomlParseException $e) {
    echo $e->getMessage();      // Duplicate key 'port' on line 2, column 1
    echo $e->getTomlLine();     // 2
    echo $e->getTomlColumn();   // 1
}
```

| Situation | Exception |
| --- | --- |
| Malformed TOML, invalid UTF-8, nesting too deep | `TomlParseException` |
| File missing, unreadable, a directory, blocked by `open_basedir` | `TomlException` |
| Empty path or a path containing NUL bytes | `ValueError` |

`toml_parse_file()` reads through PHP streams, so `open_basedir`, stream wrappers (`phar://`, `data://`, ...) and `allow_url_fopen` behave as they do for `file_get_contents()`. Parse errors from files include the path in the message. Columns count Unicode code points.

## Type mapping

| TOML | PHP |
| --- | --- |
| String | `string`, exactly as decoded (CRLF inside multi-line strings is kept) |
| Integer | `int`. Values outside the platform's `int` range are a parse error |
| Float | `float`, including `INF`, `-INF` and `NAN`. Literals that overflow a double are a parse error |
| Boolean | `bool` |
| Offset date-time | `string` such as `1979-05-27T07:32:00Z` or `1979-05-27T00:32:00.999999-07:00` |
| Local date-time | `string` such as `1979-05-27T07:32:00` |
| Local date | `string` such as `1979-05-27` |
| Local time | `string` such as `07:32:00` |
| Array | list `array` |
| Table, inline table | associative `array`, keys in document order |

Date and time values are returned as RFC 3339 strings rather than objects, which keeps parsing allocation-light and avoids inventing time zones for local values. They are normalized so they can be passed straight to `new DateTimeImmutable()`: the separator becomes `T`, `z` becomes `Z`, omitted seconds are written as `:00`, and fractional seconds and offsets are kept as written.

Keys follow PHP array semantics: a key that is a canonical decimal integer such as `1234` becomes an `int` key, while `007` or `-0` stay strings.

## Limits

- Arrays and inline tables nest at most 256 levels deep. Deeper input fails with a `TomlParseException`, and on PHP builds that track the C stack the parser also stops before running out of it.
- Input size is bounded only by `memory_limit`.
- Integers are 64-bit on 64-bit PHP builds and follow `PHP_INT_MAX` on 32-bit builds.

## Building

Unix:

```sh
phpize
./configure --enable-toml
make
make test
```

Windows, from a [PHP SDK](https://github.com/php/php-sdk-binary-tools) shell with the PHP devel pack:

```bat
phpize
configure --enable-toml=shared
nmake
```

To compile it into PHP, copy the repository to `ext/toml` in the PHP source tree, run `./buildconf --force` (`buildconf.bat` on Windows) and configure PHP with `--enable-toml` (or `--enable-toml=shared`).

[PHP-Binaries](https://github.com/Bedrock-Phanatics/PHP-Binaries) does exactly that on Linux, macOS and Windows: it clones this repository, checks out the commit pinned as `EXT_TOML_COMMIT` in `versions.env`, refuses to build if `HEAD` differs, and compiles the extension statically. Releasing a new version means pushing the commit and updating `EXT_TOML_VERSION` and `EXT_TOML_COMMIT` there.

## Testing

```sh
make test                                   # PHPT suite
php -d extension=modules/toml.so tools/toml-test.php path/to/toml-test/tests 1.1.0
php -d extension=modules/toml.so tools/fuzz.php path/to/corpus 100000
```

The PHPT suite and toml-test have been run on Linux with PHP 8.4.26 and 8.5.11 (NTS and ZTS) and on Windows with MSVC (PHP 8.4.26 NTS with the extension compiled in, ZTS as a DLL). Sanitizer, Valgrind and fuzzing runs were done on Linux. macOS is covered by CI only.

`tools/toml-test.php` runs the toml-test corpus without needing its Go runner. `tools/fuzz.php` mutates a corpus of documents and is meant to run under an ASan/UBSan PHP build with `USE_ZEND_ALLOC=0`. `tests/017-threads.phpt` parses from several pmmpthread workers and only runs on ZTS builds that load pmmpthread.

## Design

The scanner and grammar rules come from [tomlc17](https://github.com/cktan/tomlc17), but instead of building tomlc17's datum tree and converting it, the parser writes values straight into PHP arrays:

- Table bookkeeping (which tables were defined by headers, dotted keys or inline syntax) lives in a small side map keyed by the table pointer, so no flags are stored in PHP arrays and inline tables need no bookkeeping at all.
- Every distinct key is allocated once per parse and shared by all tables that use it, with its hash computed while it is scanned. Arrays of tables repeat the same keys thousands of times.
- UTF-8 is validated as strings and comments are scanned, so ASCII input pays nothing extra.
- Integer, float and date literals are validated and converted in place. Floats use `zend_strtod()`, which ignores the C locale.
- The source buffer's terminating NUL is used as a sentinel, so the hot loops have no bounds checks.

## Benchmarks

PHP 8.4.26 NTS release build, GCC 13.3, AMD Ryzen 5 5500 under WSL2, one pinned core. Times include freeing the result. `bench/data` holds the small documents, `bench/generate.php` writes `large` (a 4,000 key translation table plus 3,000 `[[items]]`) and `deep` (8 level table paths and 24 level inline nesting).

```sh
php bench/generate.php
php -d extension=modules/toml.so bench/run.php [--impl=ext|json|devium|yosymfony]
```

Median parse time (p95 in parentheses):

| Document | Size | ext-toml | tomlc17 tree, then converted | `json_decode` of the same data |
| --- | ---: | ---: | ---: | ---: |
| tiny | 136 B | 0.31 µs (0.53) | 2.60 µs (3.79) | 0.62 µs (1.08) |
| typical | 4.6 KB | 11.5 µs (17.4) | 90.2 µs (145) | 21.2 µs (36.9) |
| large | 1.2 MB | 4.32 ms (5.95) | 52.2 ms (59.7) | 7.04 ms (8.69) |
| deep | 41 KB | 287 µs (447) | 2.25 ms (3.38) | 514 µs (646) |

Heap allocations per parse, counted with Valgrind and `USE_ZEND_ALLOC=0`, and the size of the returned array:

| Document | Allocations, ext-toml / tomlc17 | Bytes allocated, ext-toml / tomlc17 | Result, ext-toml / `json_decode` |
| --- | ---: | ---: | ---: |
| tiny | 13 / 22 | 1.0 KB / 6.1 KB | 0.7 KB / 0.7 KB |
| typical | 359 / 720 | 46 KB / 84 KB | 31 KB / 33 KB |
| large | 59,058 / 173,295 | 7.8 MB / 20.0 MB | 6.3 MB / 7.9 MB |
| deep | 7,293 / 24,430 | 1.1 MB / 2.5 MB | 1.0 MB / 1.2 MB |

The pure PHP libraries are two to three orders of magnitude slower: [devium/toml](https://github.com/vanodevium/toml) 1.1.0 takes 229 µs for `tiny` and 10.4 ms for `typical`, and rejects `large` because of its uppercase hexadecimal integers. [yosymfony/toml](https://github.com/yosymfony/toml) only implements TOML 0.4 and rejects `typical`.

## License

MIT. `src/parser.c` is derived from tomlc17 by CK Tan, also MIT licensed. See [LICENSE](LICENSE).
