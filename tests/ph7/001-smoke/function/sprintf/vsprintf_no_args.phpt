--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
vsprintf() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    vsprintf();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
vsprintf() expects exactly 2 arguments, 0 given
