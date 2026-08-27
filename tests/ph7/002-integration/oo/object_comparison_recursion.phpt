--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object comparison recursion limit
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
class TestObj {
    public $ref;
}
$a = new TestObj();
$b = new TestObj();
$a->ref = $b;
$b->ref = $a;
$result = $a == $b;
?>
--EXPECTF--
Error: Nesting limit reached: Infinite recursion? in %s on line %d
--CLEAN--
<?php
unset($a, $b, $result);
