--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Class instance iteration and attribute access
--FILE--
<?php
class TestClass {
    public $public1 = 'pub1';
    public $public2 = 'pub2';
    protected $protected1 = 'prot1';
    private $private1 = 'priv1';
}

$obj = new TestClass();

// Test iteration over object properties (exercises PH7_ClassInstanceWalk)
$props = array();
foreach ($obj as $key => $value) {
    $props[$key] = $value;
}
echo "Iteration worked: " . (count($props) > 0 ? 'yes' : 'no') . "\n";

// Test that iteration works (exercises PH7_ClassInstanceWalk via foreach)
// foreach iteration over object already exercises PH7_ClassInstanceWalk

// Test attribute existence check (exercises PH7_ClassInstanceFetchAttr)
echo "public1 exists: " . (property_exists($obj, 'public1') ? 'yes' : 'no') . "\n";
echo "private1 exists: " . (property_exists($obj, 'private1') ? 'yes' : 'no') . "\n";

// Test dynamic property access (exercises PH7_ClassInstanceFetchAttr)
$dynProp = 'public2';
echo "Dynamic access: " . $obj->$dynProp . "\n";

// Test getting class name from instance
echo "get_class: " . get_class($obj) . "\n";
?>
--EXPECT--
Iteration worked: yes
public1 exists: yes
private1 exists: yes
Dynamic access: pub2
get_class: TestClass
--CLEAN--
<?php
unset($obj, $props, $dynProp);
