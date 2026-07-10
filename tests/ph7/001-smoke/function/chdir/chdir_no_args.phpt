--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chdir() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    chdir();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
chdir() expects exactly 1 argument, 0 given
