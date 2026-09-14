--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
is_callable($v, true) checks shape only, not existence
--FILE--
<?php
class IcsoC { public function m(){} }
$o = new IcsoC;
$cases = [
    ['', true],
    ['a::b', true],
    [[$o, 'm'], true],
    [[$o, 'nope'], true],
    [[$o, 123], true],
    [['IcsoC', 'm'], true],
    [[$o], true],
    [123, true],
    [$o, true],
    ['strlen', false],
    ['nope_fn', false],
];
foreach ($cases as [$v, $s]) {
    echo var_export(is_callable($v, $s), true), "\n";
}
?>
--EXPECT--
true
true
true
true
false
true
false
false
false
true
false
--CLEAN--
<?php
