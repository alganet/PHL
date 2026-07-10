--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strtr() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    strtr("x");
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
strtr() expects exactly 2 arguments, 1 given
