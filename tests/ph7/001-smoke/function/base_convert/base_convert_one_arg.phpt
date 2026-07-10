--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    base_convert("10");
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
base_convert() expects exactly 3 arguments, 1 given
