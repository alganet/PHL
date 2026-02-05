--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
var keyword for class properties
--FILE--
<?php
class TestVar {
    var $prop1 = "default";
    var $prop2 = 42;
    var $prop3;
}

$obj = new TestVar();
echo "prop1: " . $obj->prop1 . "\n";
echo "prop2: " . $obj->prop2 . "\n";
echo "prop3: " . (isset($obj->prop3) ? $obj->prop3 : "null") . "\n";
?>
--EXPECT--
prop1: default
prop2: 42
prop3: null
--CLEAN--
<?php
unset($obj);
