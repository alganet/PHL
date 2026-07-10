--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strrev() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    strrev();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
strrev() expects exactly 1 argument, 0 given
