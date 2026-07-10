--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
nl2br() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    nl2br();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
nl2br() expects at least 1 argument, 0 given
