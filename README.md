# ext-toml

A fast, lightweight [TOML 1.1](https://toml.io/en/v1.1.0) parser for PHP 8.4+, written in C17.

ext-toml parses directly into PHP arrays without building an intermediate syntax tree, minimizing memory allocations and parsing overhead.

- Passes all **712 official TOML 1.1 tests**
- Supports PHP 8.4+ (ZTS and NTS)
- Compatible with Linux, macOS and Windows
- No external dependencies or global mutable state
- Uses Zend memory management and respects `memory_limit`
- Supports static and shared builds

## Usage

```php
$config = toml_parse_file(__DIR__ . '/server.toml');
$config = toml_parse('port = 19132');
```

The extension provides two functions:

```php
toml_parse(string $source): array
toml_parse_file(string $path): array
```

TOML tables become associative PHP arrays, while TOML arrays become indexed arrays. Strings, integers, floats and booleans map to their corresponding PHP types. Date and time values are returned as normalized strings.

Invalid TOML throws `TomlParseException`, which provides `getTomlLine()` and `getTomlColumn()` for error reporting. File access failures throw `TomlException`.

File parsing uses PHP streams and respects `open_basedir` and other standard PHP stream restrictions.

## Benchmarks

Benchmarked on PHP 8.4.26 NTS, GCC 13.3 and an AMD Ryzen 5 5500 under WSL2, using a single pinned CPU core.

### Parsing performance

Median execution time, including cleanup. **Lower is better.**

| Document | ext-toml | tomlc17 | Speedup |
|---|---:|---:|---:|
| Tiny (136 B) | **0.31 µs** | 2.60 µs | **8.4x** |
| Typical (4.6 KB) | **11.5 µs** | 90.2 µs | **7.8x** |
| Large (1.2 MB) | **4.32 ms** | 52.2 ms | **12.1x** |
| Deep (41 KB) | **287 µs** | 2,250 µs | **7.8x** |

The tomlc17 results include parsing into its native tree and converting the result to PHP arrays.

For reference, `json_decode()` processes equivalent data in 0.62 µs, 21.2 µs, 7.04 ms and 514 µs, respectively.

### Compared to YAML

The same data re-encoded with `yaml_emit()` and parsed with `yaml_parse()`, using [pecl yaml](https://github.com/php/pecl-file_formats-yaml) 2.3.0 on libyaml 0.2.5, the versions [PHP-Binaries](https://github.com/Bedrock-Phanatics/PHP-Binaries) ships for PocketMine-MP. **Lower is better.**

| Document | ext-toml | yaml_parse | Speedup |
|---|---:|---:|---:|
| Tiny | **0.31 µs** | 4.56 µs | **14.7x** |
| Typical | **11.5 µs** | 168 µs | **14.6x** |
| Large | **4.32 ms** | 38.4 ms | **8.9x** |
| Deep | **287 µs** | 2,650 µs | **9.2x** |

### Memory usage

Total allocated memory per parse, measured with Valgrind and `USE_ZEND_ALLOC=0`.

| Document | ext-toml | tomlc17 | Reduction |
|---|---:|---:|---:|
| Tiny | **1.0 KB** | 6.1 KB | **84%** |
| Typical | **46 KB** | 84 KB | **45%** |
| Large | **7.8 MB** | 20.0 MB | **61%** |
| Deep | **1.1 MB** | 2.5 MB | **56%** |

### Heap allocations

Number of heap allocations per parse.

| Document | ext-toml | tomlc17 | Reduction |
|---|---:|---:|---:|
| Tiny | **13** | 22 | **41%** |
| Typical | **359** | 720 | **50%** |
| Large | **59,058** | 173,295 | **66%** |
| Deep | **7,293** | 24,430 | **70%** |

These improvements come primarily from constructing PHP values directly rather than allocating and converting an intermediate tree.

Pure PHP alternatives are substantially slower on the tested workloads. [devium/toml](https://github.com/vanodevium/toml) takes 229 µs for the tiny document and 10.4 ms for the typical document.

To reproduce the benchmarks:

```sh
php bench/generate.php
php -d extension=modules/toml.so bench/run.php
```

The benchmark runner also supports `--impl=ext|json|yaml|devium|yosymfony`.

## Building

**Linux / macOS**

```sh
phpize
./configure --enable-toml
make
make test
```

**Windows**

Using a [PHP SDK](https://github.com/php/php-sdk-binary-tools) development environment:

```bat
phpize
configure --enable-toml=shared
nmake
```

The extension can also be compiled directly into PHP by placing the repository in `ext/toml`, regenerating the build configuration and enabling it with `--enable-toml`.

[PHP-Binaries](https://github.com/Bedrock-Phanatics/PHP-Binaries) builds the extension statically on Linux, macOS and Windows using a pinned commit.

## Testing

The repository includes PHPT tests, the official TOML compliance suite and a mutation-based fuzzer.

```sh
make test
php -d extension=modules/toml.so tools/toml-test.php path/to/toml-test/tests 1.1.0
php -d extension=modules/toml.so tools/fuzz.php path/to/corpus 100000
```

The extension has been tested on PHP 8.4 and 8.5 across Linux and Windows, including ZTS and NTS configurations. Additional sanitizer, Valgrind and fuzzing tests were performed on Linux. macOS is covered by CI.

Arrays and inline tables are limited to 256 levels of nesting to prevent excessive recursion. Input size is otherwise governed by PHP's memory limit.

## Implementation

The parser is based on [tomlc17](https://github.com/cktan/tomlc17), with its output handling adapted for PHP.

It constructs PHP arrays directly, interns repeated keys within each parse, validates UTF-8 during scanning and converts numeric values without temporary syntax nodes.

Floats are parsed using `zend_strtod()` for locale-independent conversion. All allocations use the Zend allocator.

## License

MIT. The parser is derived from [tomlc17](https://github.com/cktan/tomlc17) by CK Tan, also MIT licensed. See [LICENSE](LICENSE).
