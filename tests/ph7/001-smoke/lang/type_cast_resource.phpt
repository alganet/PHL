--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Type casting of resource values (was a bare skip freezing PHL's old behavior: every resource cast to int 1 and stringified to ResourceID_0x<pointer>)
--FILE--
<?php
$fp = fopen(__FILE__, 'r');
$id = get_resource_id($fp);

// php casts a resource to its ID, not to a truthy 1.
echo "int:      ", var_export((int)$fp === $id, true), "\n";
echo "float:    ", var_export((float)$fp === (float)$id, true), "\n";
echo "bool:     ", var_export((bool)$fp === true, true), "\n";
echo "string:   ", var_export((string)$fp === "Resource id #$id", true), "\n";
echo "interp:   ", var_export("$fp" === "Resource id #$id", true), "\n";
echo "array:    ", var_export(is_array((array)$fp), true), "\n";
echo "object:   ", var_export(is_object((object)$fp), true), "\n";

// Two live resources are DISTINCT: they used to compare equal, both casting to 1.
$fp2 = fopen(__FILE__, 'r');
echo "distinct: ", var_export($fp == $fp2, true), "\n";
echo "self:     ", var_export($fp == $fp, true), "\n";

echo "type:     ", gettype($fp), " / ", get_resource_type($fp), "\n";
fclose($fp);
fclose($fp2);
echo "closed:   ", gettype($fp), "\n";
?>
--EXPECT--
int:      true
float:    true
bool:     true
string:   true
interp:   true
array:    true
object:   true
distinct: false
self:     true
type:     resource / stream
closed:   resource (closed)
--CLEAN--
<?php
unset($fp, $fp2, $id);
