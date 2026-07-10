--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
htmlspecialchars_decode() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    htmlspecialchars_decode();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
htmlspecialchars_decode() expects at least 1 argument, 0 given
