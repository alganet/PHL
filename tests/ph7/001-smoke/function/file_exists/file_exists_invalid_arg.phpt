--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
file_exists() rejects a non-string filename with php's TypeError
--FILE--
<?php
try {
    file_exists(array());
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
file_exists(): Argument #1 ($filename) must be of type string, array given
--CLEAN--
<?php
