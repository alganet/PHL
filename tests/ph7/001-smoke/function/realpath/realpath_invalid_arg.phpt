--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
realpath() rejects a non-string path with php's TypeError
--FILE--
<?php
try {
    realpath(array());
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
realpath(): Argument #1 ($path) must be of type string, array given
--CLEAN--
<?php
