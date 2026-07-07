--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fmod(5.0) throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    fmod(5.0);
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
fmod() expects exactly 2 arguments, 1 given
