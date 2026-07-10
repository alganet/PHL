--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sprintf() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    sprintf();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
sprintf() expects at least 1 argument, 0 given
