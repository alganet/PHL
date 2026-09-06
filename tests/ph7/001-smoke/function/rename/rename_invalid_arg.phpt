--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
rename() rejects a non-string destination with php's TypeError
--FILE--
<?php
try {
    rename('only_one_arg', array());
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
rename(): Argument #2 ($to) must be of type string, array given
--CLEAN--
<?php
