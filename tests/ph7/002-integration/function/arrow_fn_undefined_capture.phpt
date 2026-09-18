--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An arrow fn does not capture a variable undefined at creation; a body read of it warns
--DESCRIPTION--
An arrow function `fn()=>expr` auto-captures its free variables BY VALUE at creation, but php
does NOT capture one that is undefined at that point — the isolated body scope then has no such
variable, so reading it there raises E_WARNING "Undefined variable" at the READ (not at
creation), reflection's getClosureUsedVariables omits it, and a later assignment to the outer
variable does not retro-capture. PHL used to install the missing capture as NULL, so the body
saw it defined and stayed silent (and reflection listed it). Two coordinated fixes: skip
installing an undefined arrow capture (VmExecOpLoadClosure), and compile the arrow body
read-only (EXPR_FLAG_RDONLY_LOAD) so a lone undefined variable warns instead of loading quietly
like a bare `$z;` statement. The handler normalizes the warning prefix/stream across engines.
--FILE--
<?php
set_error_handler(function ($no, $str, $file, $line) { echo "[$no L$line] $str\n"; return true; });

echo "== not called: no capture attempt, silent ==\n";
$a1 = fn () => $af_never;
echo "made\n";

echo "== called, undefined at creation: warns at body READ, returns null ==\n";
$a2 = fn () => $af_missing;
var_dump($a2());

echo "== assigned AFTER creation does not retro-capture (isolated scope) ==\n";
$a3 = fn () => $af_late;
$af_late = 5;
var_dump($a3());

echo "== defined BEFORE creation: captured by value, no warning ==\n";
$af_have = 7;
$a4 = fn () => $af_have;
var_dump($a4());

echo "== mixed: defined captured, undefined warns at read ==\n";
$af_x = 1;
$a5 = fn () => $af_x + $af_y;
var_dump($a5());

echo "== reflection omits an undefined-at-creation capture ==\n";
$a6 = fn () => $af_refl;
var_dump((new ReflectionFunction($a6))->getClosureUsedVariables());

echo "== assignment inside the body still creates and reads ==\n";
$a7 = fn () => ($af_asg = 5) + $af_asg;
var_dump($a7());

echo "== nested arrow, undefined in the inner body ==\n";
$a8 = fn () => fn () => $af_nested;
var_dump($a8()());
?>
--EXPECT--
== not called: no capture attempt, silent ==
made
== called, undefined at creation: warns at body READ, returns null ==
[2 L9] Undefined variable $af_missing
NULL
== assigned AFTER creation does not retro-capture (isolated scope) ==
[2 L13] Undefined variable $af_late
NULL
== defined BEFORE creation: captured by value, no warning ==
int(7)
== mixed: defined captured, undefined warns at read ==
[2 L24] Undefined variable $af_y
int(1)
== reflection omits an undefined-at-creation capture ==
array(0) {
}
== assignment inside the body still creates and reads ==
int(10)
== nested arrow, undefined in the inner body ==
[2 L36] Undefined variable $af_nested
NULL
