--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: type coercion with type hints
--FILE--
<?php
function natcof(int $a, string $b) {
    echo (is_int($a) ? "int" : "?") . ":$a ";
    echo (is_string($b) ? "string" : "?") . ":$b\n";
}
natcof(b: 42, a: "7");
natcof(a: 3, b: 100);
?>
--EXPECT--
int:7 string:42
int:3 string:100
--CLEAN--
<?php
