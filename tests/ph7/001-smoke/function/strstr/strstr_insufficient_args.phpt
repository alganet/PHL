--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strstr('abc') throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    strstr('abc');
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
strstr() expects at least 2 arguments, 1 given
