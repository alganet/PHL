--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: trait_exists reports only traits, not classes/interfaces/enums
--FILE--
<?php
trait TexTrait { public function m() {} }
interface TexIface {}
class TexClass {}
enum TexEnum {}
$texOut = [];
$texOut[] = var_export(trait_exists('TexTrait'), true);
$texOut[] = var_export(trait_exists('TexClass'), true);
$texOut[] = var_export(trait_exists('TexIface'), true);
$texOut[] = var_export(trait_exists('TexEnum'), true);
$texOut[] = var_export(trait_exists('TexNope'), true);
$texOut[] = var_export(trait_exists('TexNope', false), true);
echo implode(' ', $texOut), "\n";
// the other *_exists predicates must NOT report a trait
echo var_export(class_exists('TexTrait'), true), " ",
     var_export(interface_exists('TexTrait'), true), "\n";
?>
--EXPECT--
true false false false false false
false false
