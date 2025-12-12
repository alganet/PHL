--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_class_methods builtin basic checks
--SKIPIF--
<?php
if (function_exists('zend_version')) { echo "skip: PHL only\n"; }
if (!function_exists('get_class_methods')) { echo "skip: function not available\n"; }
?>
--FILE--
<?php
class A {
    public function f1(){}
    protected function f2(){}
    private function f3(){}
}
// public methods only by default
$methods = get_class_methods('A');
echo in_array('f1', $methods) ? "ok\n" : "fail\n";
// f2 is protected, should not be returned when called with class name
// But when called with object, returns public methods only as well
$methods_obj = get_class_methods(new A);
echo in_array('f2', $methods_obj) ? "ok\n" : "fail\n";
// non-existent class should return null
$none = get_class_methods('NonExistent');
echo ($none === null) ? "ok\n" : "fail\n";
?>
--EXPECT--
ok
ok
ok
