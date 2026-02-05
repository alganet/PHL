--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
Duplicate method in class triggers overloading path
--FILE--
<?php
class Test {
    function foo() { return 'first'; }
    function foo() { return 'second'; }
}
$o = new Test;
echo $o->foo();
?>
--EXPECT--
first
--CLEAN--
<?php
unset($o);
