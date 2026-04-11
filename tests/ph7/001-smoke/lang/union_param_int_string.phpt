--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Union type parameter: int|string accepts both
--FILE--
<?php
function upis_f(int|string $x) {
    echo is_int($x) ? "int" : "str", ":", $x, "\n";
}
upis_f(42);
upis_f("hi");
?>
--EXPECT--
int:42
str:hi
--CLEAN--
<?php
