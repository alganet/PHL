--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ctype_punct() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    ctype_punct();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
ctype_punct() expects exactly 1 argument, 0 given
