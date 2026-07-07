--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strncasecmp("test") throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    strncasecmp("test");
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
strncasecmp() expects exactly 3 arguments, 1 given
