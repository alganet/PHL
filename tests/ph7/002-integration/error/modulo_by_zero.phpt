--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: Modulo by zero throws a catchable DivisionByZeroError
--FILE--
<?php
try {
    $result = 10 % 0;
    echo "Result: $result\n";
} catch (DivisionByZeroError $e) {
    echo get_class($e), ": ", $e->getMessage(), "\n";
}
echo "continues\n";
?>
--EXPECT--
DivisionByZeroError: Modulo by zero
continues
--CLEAN--
<?php
