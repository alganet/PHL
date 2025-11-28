--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: Function overloading (different arity)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
function foo(){ echo "zero\n"; }
function foo($a){ echo "one\n"; }
function foo($a,$b){ echo "two\n"; }

foo();
foo(1);
foo(1,2);
?>
--EXPECT--
zero
one
two

--CLEAN--
<?php
// no cleanup required
?>
