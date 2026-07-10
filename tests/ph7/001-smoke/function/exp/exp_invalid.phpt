--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
exp() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    exp();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
exp() expects exactly 1 argument, 0 given
