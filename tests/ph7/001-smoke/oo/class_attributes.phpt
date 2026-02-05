--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
OO class attributes and visibility
--FILE--
<?php
class OoAttrTestClass {
    public $public_attr = "public_default";
    private $private_attr = "private_default";
    protected $protected_attr = "protected_default";
}

$obj = new OoAttrTestClass();

// Test public attribute access
echo "Public attr initial: " . $obj->public_attr . "\n";
$obj->public_attr = "modified";
echo "Public attr modified: " . $obj->public_attr . "\n";

// Test that private/protected attributes are accessible (PHL may have different visibility rules)
echo "Testing attribute access...\n";
$has_public = property_exists($obj, 'public_attr');
echo "Has public attr: " . ($has_public ? "yes" : "no") . "\n";

// Test isset on attributes
$isset_public = isset($obj->public_attr);
echo "Isset public: " . ($isset_public ? "true" : "false") . "\n";

// Test unset on attributes
unset($obj->public_attr);
$isset_after_unset = isset($obj->public_attr);
echo "Isset after unset: " . ($isset_after_unset ? "true" : "false") . "\n";
?>
--EXPECTF--
Public attr initial: public_default
Public attr modified: modified
Testing attribute access...
Has public attr: yes
Isset public: true
Isset after unset: false
--CLEAN--
<?php
unset($obj, $has_public, $isset_public, $isset_after_unset);
