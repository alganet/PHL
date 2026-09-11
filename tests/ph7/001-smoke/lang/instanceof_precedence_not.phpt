--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
instanceof binds tighter than logical NOT: !$x instanceof C is !($x instanceof C)
--FILE--
<?php
class IopnWrap {}
$s = 'hello';
// Without parens, php parses !$s instanceof C as !($s instanceof C) = !false = true
if (!$s instanceof IopnWrap) { $s = 'CHANGED'; }
echo $s, "\n";
// The PHPUnit constructor shape: wrap non-objects
$out = [];
foreach (['a', new IopnWrap()] as $item) {
    if (!$item instanceof IopnWrap) { $item = new IopnWrap(); }
    $out[] = get_class($item);
}
echo implode(',', $out), "\n";
// Assignment form
$obj = new IopnWrap();
$cond = !$obj instanceof IopnWrap;
echo $cond ? "true\n" : "false\n";
?>
--EXPECT--
CHANGED
IopnWrap,IopnWrap
false
