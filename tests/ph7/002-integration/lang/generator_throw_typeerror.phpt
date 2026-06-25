--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator::throw() raises a TypeError when the argument is not a Throwable
--FILE--
<?php
class Plain {}
function gen() { yield 1; }
foreach ([ "nope", null, 42, new Plain() ] as $bad) {
    $g = gen();
    $g->current();
    try {
        $g->throw($bad);
    } catch (\TypeError $e) {
        echo $e->getMessage(), "\n";
    }
}
?>
--EXPECT--
Generator::throw(): Argument #1 ($exception) must be of type Throwable, string given
Generator::throw(): Argument #1 ($exception) must be of type Throwable, null given
Generator::throw(): Argument #1 ($exception) must be of type Throwable, int given
Generator::throw(): Argument #1 ($exception) must be of type Throwable, Plain given
--CLEAN--
<?php
