--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object comparison functionality
--FILE--
<?php
class TestClass {
    public $attr = "value";
}

$obj1 = new TestClass();
$obj2 = new TestClass();
$obj3 = $obj1; // Same instance

// Test identical objects
$identical = ($obj1 === $obj3);
echo $identical ? "identical_ok" : "identical_fail";
echo "\n";

// Test equal objects
$equal = ($obj1 == $obj2);
echo $equal ? "equal_ok" : "equal_fail";
echo "\n";

// Test different objects
$different = ($obj1 !== $obj2);
echo $different ? "different_ok" : "different_fail";
?>
--EXPECT--
identical_ok
equal_ok
different_ok
--CLEAN--
<?php
unset($obj1, $obj2, $obj3, $identical, $equal, $different);
