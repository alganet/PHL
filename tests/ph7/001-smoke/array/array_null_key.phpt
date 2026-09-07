--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array with null key (converted to empty string)
--FILE--
<?php
// Test array with null key - should be treated as empty string
$arr = array(null => "value");
echo "Array with null key: " . $arr[""] . "\n";
echo "Count: " . count($arr) . "\n";
?>
--EXPECTF--
%AUsing null as an array offset is deprecated, use an empty string instead%AArray with null key: value%ACount: 1%A
--CLEAN--
<?php
unset($arr);
