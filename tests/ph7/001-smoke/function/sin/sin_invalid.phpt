--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sin() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    sin();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
sin() expects exactly 1 argument, 0 given
