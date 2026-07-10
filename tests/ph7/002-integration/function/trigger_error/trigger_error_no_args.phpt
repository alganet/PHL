--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
trigger_error() throws ArgumentCountError with too few arguments (PHP 8)
--FILE--
<?php
try {
    trigger_error();
} catch (\ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
trigger_error() expects at least 1 argument, 0 given
