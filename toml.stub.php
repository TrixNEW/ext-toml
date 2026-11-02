<?php

/** @generate-class-entries */

/** @refcount 1 */
function toml_parse(string $source): array {}

/** @refcount 1 */
function toml_parse_file(string $path): array {}

/** @refcount 1 */
function toml_encode(array $data): string {}

class TomlException extends Exception
{
}

final class TomlParseException extends TomlException
{
    private int $tomlLine = 0;
    private int $tomlColumn = 0;

    public function getTomlLine(): int {}

    public function getTomlColumn(): int {}
}
