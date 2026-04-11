--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Union type on a variadic parameter accepts mixed allowed types
--FILE--
<?php
function uv_f(int|string ...$xs) {
    foreach ($xs as $x) {
        echo is_int($x) ? "int:" : "str:", $x, "\n";
    }
}
uv_f(1, "two", 3, "four");
?>
--EXPECT--
int:1
str:two
int:3
str:four
--CLEAN--
<?php
