--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: Cannot overwrite a final method in subclass
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip";
}
?>
--FILE--
<?php
class A {
    final public function f() { return 1; }
}
class B extends A {
    public function f() { return 2; }
}
?>
--EXPECTF--
%s 6 Error: Cannot Overwrite final method 'A:f' inside child class 'B'
Compile error
