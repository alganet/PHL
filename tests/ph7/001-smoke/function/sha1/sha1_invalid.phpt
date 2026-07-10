--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sha1() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    sha1();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
sha1() expects at least 1 argument, 0 given
