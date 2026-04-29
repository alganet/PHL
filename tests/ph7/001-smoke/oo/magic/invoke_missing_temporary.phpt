--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Calling a temporary object without __invoke raises a catchable Error
--FILE--
<?php
class PlainTemp {}
for ($i = 0; $i < 5; $i++) {
    try {
        (new PlainTemp())(1, 2, 3);
        echo "no throw\n";
    } catch (Error $e) {
        echo $e->getMessage(), "\n";
    }
}
?>
--EXPECT--
Object of type PlainTemp is not callable
Object of type PlainTemp is not callable
Object of type PlainTemp is not callable
Object of type PlainTemp is not callable
Object of type PlainTemp is not callable
--CLEAN--
<?php
