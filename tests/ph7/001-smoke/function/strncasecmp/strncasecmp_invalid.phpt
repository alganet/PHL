--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strncasecmp() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    strncasecmp();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
strncasecmp() expects exactly 3 arguments, 0 given
