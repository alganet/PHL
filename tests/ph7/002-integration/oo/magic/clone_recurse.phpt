--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object clone nesting limit triggers error
--SKIPIF--
<?php
// PHL caps recursive __clone() invocation ("Object clone limit reached");
// php has no clone-depth limit at all. Engine-design difference, not a fidelity gap.
if (function_exists('zend_version')) { echo 'skip PHL caps recursive __clone(); php has no clone limit'; }
?>
--FILE--
<?php
class A {
    public $x;
    function __clone() {
        $this->x = clone $this;
    }
}
$a = new A;
$b = clone $a;
?>
--EXPECTF--
Error: Object clone limit reached,no more call to __clone() in %s on line %d
--CLEAN--
<?php
unset($a, $b);
