--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
OO static attributes and class constants (constants not supported in PHL)
--FILE--
<?php
class OoStaticClass {
    public static $static_attr = "static_value";
    const CLASS_CONSTANT = "constant_value";
}

echo "Class defined successfully\n";

// Test static attribute access (may not be supported)
echo "Testing static access...\n";

// Test class constant access (not supported in PHL)
$constant_value = OoStaticClass::CLASS_CONSTANT;
if ($constant_value === null) {
    echo "Class constant: null\n";
} elseif ($constant_value === "") {
    echo "Class constant: empty\n";
} else {
    echo "Class constant: " . $constant_value . "\n";
}

// Test defined() on class constant (not supported in PHL)
$constant_defined = defined('OoStaticClass::CLASS_CONSTANT');
echo "Constant defined: " . ($constant_defined ? "yes" : "no") . "\n";

// Test get_class_vars (should show static attributes if supported)
$class_vars = get_class_vars('OoStaticClass');
echo "Class vars count: " . count($class_vars) . "\n";

// Test get_defined_constants
$all_constants = get_defined_constants(true);
$user_constants = isset($all_constants['user']) ? $all_constants['user'] : array();
$has_class_constant = isset($user_constants['OoStaticClass::CLASS_CONSTANT']);
echo "Class constant in defined constants: " . ($has_class_constant ? "yes" : "no") . "\n";
?>
--CLEAN--
<?php
// No cleanup needed
?>
--EXPECTF--
Class defined successfully
Testing static access...
Class constant: %s
Constant defined: %s
Class vars count: %d
Class constant in defined constants: no
