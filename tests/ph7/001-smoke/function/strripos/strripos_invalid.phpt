--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strripos() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    strripos();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
strripos() expects at least 2 arguments, 0 given
