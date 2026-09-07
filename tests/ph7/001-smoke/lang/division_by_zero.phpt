--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Modulo with a non-numeric string is a TypeError
--FILE--
<?php
try {
    echo "a" % 2;
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
Unsupported operand types: string % int
--CLEAN--
<?php
