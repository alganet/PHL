--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr("hello") throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    substr("hello");
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
substr() expects at least 2 arguments, 1 given
