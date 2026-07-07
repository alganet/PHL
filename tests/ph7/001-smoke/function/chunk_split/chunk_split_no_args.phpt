--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chunk_split() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    chunk_split();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
chunk_split() expects at least 1 argument, 0 given
