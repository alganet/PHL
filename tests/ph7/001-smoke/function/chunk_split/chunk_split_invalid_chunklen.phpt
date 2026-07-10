--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
chunk_split with a non-positive chunk length throws ValueError
--FILE--
<?php
try {
    chunk_split("hello", 0);
    echo "NO_ERROR\n";
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
chunk_split(): Argument #2 ($length) must be greater than 0
--CLEAN--
<?php
