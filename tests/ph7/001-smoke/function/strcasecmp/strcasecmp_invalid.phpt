--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strcasecmp() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    strcasecmp();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
strcasecmp() expects exactly 2 arguments, 0 given
