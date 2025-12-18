--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_callable with callable arrays
--SKIPIF--
<?php if (!function_exists('is_callable')) { die('skip'); } ?>
--FILE--
<?php
// Test callable array with class and method
class TestClass {
    public static function testMethod() {
        return "called";
    }
}

$instance = new TestClass();

// Test valid callable array
$callable1 = array($instance, 'testMethod');
echo is_callable($callable1) ? "callable1_ok\n" : "callable1_fail\n";

// Test callable array with class name and static method
$callable2 = array('TestClass', 'testMethod');
echo is_callable($callable2) ? "callable2_ok\n" : "callable2_fail\n";

// Test invalid callable array (wrong method name)
$callable3 = array($instance, 'nonexistent');
echo !is_callable($callable3) ? "callable3_ok\n" : "callable3_fail\n";

// Test invalid callable array (not an array)
$callable4 = "not_an_array";
echo !is_callable($callable4) ? "callable4_ok\n" : "callable4_fail\n";

// Test valid function name string
$callable5 = 'is_callable';
echo is_callable($callable5) ? "callable5_ok\n" : "callable5_fail\n";

// Test invalid function name
$callable6 = 'nonexistent_function';
echo !is_callable($callable6) ? "callable6_ok\n" : "callable6_fail\n";
?>
--CLEAN--
<?php
unset($instance, $callable1, $callable2, $callable3, $callable4, $callable5, $callable6);
?>
--EXPECT--
callable1_ok
callable2_ok
callable3_ok
callable4_ok
callable5_ok
callable6_ok