--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mkdir() rejects a non-string directory with php's TypeError
--FILE--
<?php
try {
    mkdir(array());
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
mkdir(): Argument #1 ($directory) must be of type string, array given
--CLEAN--
<?php
