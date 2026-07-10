--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ctype_cntrl() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    ctype_cntrl();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
ctype_cntrl() expects exactly 1 argument, 0 given
