--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strcspn() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    strcspn();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
strcspn() expects at least 2 arguments, 0 given
