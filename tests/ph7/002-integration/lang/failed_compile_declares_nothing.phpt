--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A unit that fails to parse declares none of the functions, classes, interfaces or traits written above the error
--FILE--
<?php
class Base {}
function kept() { return 'kept'; }

try { eval('function f1(){} class C1 extends Base {} interface I1 {} interface I2 extends I1 {} trait T1 {} $x = ;'); }
catch (ParseError $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
var_dump(function_exists('f1'), class_exists('C1'), interface_exists('I1'), interface_exists('I2'), trait_exists('T1'));

// the names are free: declaring them again is no redeclaration
eval('function f1() { return "f1"; } class C1 {}');
echo f1(), "\n";
var_dump(get_parent_class(new C1), is_subclass_of('C1', 'Base'));

// what was declared before the failed unit survives it
try { eval('function kept2(){} $x = ;'); } catch (ParseError $e) { echo $e->getMessage(), "\n"; }
echo kept(), "\n";
var_dump(function_exists('kept2'));

// include takes the same path
$file = tempnam(sys_get_temp_dir(), 'phl');
file_put_contents($file, '<?php function inc_f(){} class IncC {} $x = ;');
try { include $file; } catch (ParseError $e) { echo $e->getMessage(), "\n"; }
unlink($file);
var_dump(function_exists('inc_f'), class_exists('IncC'));
?>
--EXPECT--
ParseError: syntax error, unexpected token ";"
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
f1
bool(false)
bool(false)
syntax error, unexpected token ";"
kept
bool(false)
syntax error, unexpected token ";"
bool(false)
bool(false)
