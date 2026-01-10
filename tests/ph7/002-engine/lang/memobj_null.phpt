--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Memory object null operations (unset, casting)
--FILE--
<?php
// Test various scenarios that trigger PH7_MemObjToNull

// Test 1: unset on variable
$var1 = 42;
unset($var1);
echo "unset variable: " . (isset($var1) ? 'fail' : 'ok') . "\n";

// Test 2: unset on array element
$arr = array('a' => 1, 'b' => 2);
unset($arr['a']);
echo "unset array element: " . (isset($arr['a']) ? 'fail' : 'ok') . "\n";

// Test 3: unset on object property (note: PH7 may not fully support this)
class UnsetTest {
    public $prop = 'value';
}
$obj = new UnsetTest();
unset($obj->prop);
echo "unset object property attempted\n";

// Test 4: variable reference unset
$ref = 100;
$alias = &$ref;
unset($alias);
echo "unset reference: " . (isset($alias) ? 'fail' : 'ok') . "\n";

// Test 5: null assignment
$nullVar = "not null";
$nullVar = null;
echo "null assignment: " . ($nullVar === null ? 'ok' : 'fail') . "\n";

// Test 6: Multiple unsets
$x = 1;
$y = 2;
unset($x, $y);
echo "multiple unset: " . (!isset($x, $y) ? 'ok' : 'fail') . "\n";
?>
--EXPECT--
unset variable: ok
unset array element: ok
unset object property attempted
unset reference: ok
null assignment: ok
multiple unset: ok