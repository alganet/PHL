--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strspn() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    strspn();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
strspn() expects at least 2 arguments, 0 given
