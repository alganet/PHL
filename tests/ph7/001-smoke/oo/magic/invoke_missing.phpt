--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Calling an object without __invoke throws a catchable Error
--FILE--
<?php
class Plain {}
try {
    (new Plain())(1, 2, 3);
    echo "no throw\n";
} catch (Error $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
caught: Object of type Plain is not callable
--CLEAN--
<?php
