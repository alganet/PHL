--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_declared_classes/interfaces/traits split by KIND, report aliases folded, in declaration order
--FILE--
<?php
interface DclIface {}
trait DclTrait {}
class DclClass implements DclIface { use DclTrait; }
enum DclEnum: string { case A = 'a'; }
interface DclIfaceTwo extends DclIface {}
trait DclTraitTwo {}
class DclClassTwo extends DclClass {}

class_alias('DclClassTwo', 'DclClassAlias');
class_alias('DclIfaceTwo', 'DclIfaceAlias');
class_alias('DclTraitTwo', 'DclTraitAlias');

$mine = fn(array $a) => array_values(array_filter($a, fn($n) => str_starts_with(strtolower($n), 'dcl')));

print_r($mine(get_declared_classes()));
print_r($mine(get_declared_interfaces()));
print_r($mine(get_declared_traits()));
?>
--EXPECT--
Array
(
    [0] => DclClass
    [1] => DclEnum
    [2] => DclClassTwo
    [3] => dclclassalias
)
Array
(
    [0] => DclIface
    [1] => DclIfaceTwo
    [2] => dclifacealias
)
Array
(
    [0] => DclTrait
    [1] => DclTraitTwo
    [2] => dcltraitalias
)
--CLEAN--
<?php
