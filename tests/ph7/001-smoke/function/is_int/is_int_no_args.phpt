--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_int() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    is_int();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
is_int() expects exactly 1 argument, 0 given
