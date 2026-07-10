--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
quotemeta() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    quotemeta();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
quotemeta() expects exactly 1 argument, 0 given
