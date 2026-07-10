--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base64_decode() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    base64_decode();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
base64_decode() expects at least 1 argument, 0 given
