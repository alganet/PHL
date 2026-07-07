--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strpos() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    strpos();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
strpos() expects at least 2 arguments, 0 given
