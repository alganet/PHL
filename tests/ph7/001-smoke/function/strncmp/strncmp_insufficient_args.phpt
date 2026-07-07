--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strncmp("abc", "abc") throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    strncmp("abc", "abc");
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
strncmp() expects exactly 3 arguments, 2 given
