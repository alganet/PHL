--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
foreach: an `as` that is part of a NAME is not the separator
--FILE--
<?php
/* php lets `as` be a variable, a property and a class-constant name; only the
 * one standing on its own is foreach's separator. */
class FeAsHolder {
    const as = ['ca'];
    public $as = ['pa'];
}
$as = ['va'];
foreach ($as as $v) { echo $v, "\n"; }
$o = new FeAsHolder;
foreach ($o->as as $v) { echo $v, "\n"; }
foreach (FeAsHolder::as as $v) { echo $v, "\n"; }
foreach ($o?->as as $v) { echo $v, "\n"; }
$null = null;
foreach ($null?->as ?? ['ns'] as $v) { echo $v, "\n"; }

/* the name is fine everywhere else, too */
$as = 5;
echo $as, "\n";
foreach ([1] as $as) { echo $as, "\n"; }
$arr = ['as' => ['key']];
foreach ($arr['as'] as $v) { echo $v, "\n"; }
foreach ($arr as $k => $v) { echo $k, "\n"; }

/* and the ordinary shapes still split where they should */
$rows = [[1, 2]];
foreach ($rows as [$x, $y]) { echo $x, $y, "\n"; }
$ref = [1];
foreach ($ref as &$r) { $r = 9; }
unset($r);
echo $ref[0], "\n";
$nest = ['a' => ['b']];
foreach ($nest as $k => $inner) { foreach ($inner as $v) { echo $k, $v, "\n"; } }
foreach ([1] as $v): echo $v, "\n"; endforeach;
--EXPECT--
va
pa
ca
pa
ns
5
1
key
as
12
9
ab
1
