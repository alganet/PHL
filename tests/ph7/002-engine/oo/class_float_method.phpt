--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Class with method using float
--FILE--
<?php
// Test class with method using float
// Covers OO functionality (oo.c various lines)

class Test {
    public function getPi() {
        return 3.14159;
    }
}

$o = new Test();
echo $o->getPi();
?>
--EXPECT--
3.14159
--CLEAN--
<?php
unset($o);
?>