--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base64_encode() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    base64_encode();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
base64_encode() expects exactly 1 argument, 0 given
