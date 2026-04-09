--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object type hint accepts any object instance
--FILE--
<?php
class ObjHintA { public $name = "ObjHintA"; }
class ObjHintB extends ObjHintA { public $name = "ObjHintB"; }

function objHintGetName(object $o): string {
    return get_class($o);
}

echo objHintGetName(new ObjHintA()) . "\n";
echo objHintGetName(new ObjHintB()) . "\n";

function objHintIdentity(object $o): object {
    return $o;
}
$objHintResult = objHintIdentity(new ObjHintA());
echo $objHintResult->name . "\n";

function objHintMaybe(?object $o): string {
    if ($o === null) return "null";
    return get_class($o);
}
echo objHintMaybe(new ObjHintA()) . "\n";
echo objHintMaybe(null) . "\n";

function objHintVariadic(object ...$objs): int {
    return count($objs);
}
echo objHintVariadic(new ObjHintA(), new ObjHintB()) . "\n";

function objHintNullableVariadic(?object ...$objs): string {
    $objHintParts = [];
    foreach ($objs as $objHintItem) {
        $objHintParts[] = $objHintItem === null ? "null" : get_class($objHintItem);
    }
    return implode(",", $objHintParts);
}
echo objHintNullableVariadic(new ObjHintA(), null, new ObjHintB()) . "\n";
echo objHintNullableVariadic(null, null) . "\n";
?>
--EXPECT--
ObjHintA
ObjHintB
ObjHintA
ObjHintA
null
2
ObjHintA,null,ObjHintB
null,null
--CLEAN--
<?php
unset($objHintResult);
