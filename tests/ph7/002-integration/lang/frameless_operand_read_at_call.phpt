--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A frameless call reads a defined variable operand at the call, after later arguments ran
--FILE--
<?php
/* php compiles max(), in_array(), substr() and the like into one instruction
 * that reads a plain variable operand itself, when it runs -- so a later
 * argument that writes the variable changes what the function sees. The
 * same holds for array_key_exists() and a sprintf() php rewrites. An ordinary
 * call, or a namespace's own function of that name, sends the value it had. */
namespace {
function flr_set($v) { global $x; $x = $v; return 1; }
function flr_def() { global $u; $u = 7; return 1; }
function flr_plain($a, $b) { return $a; }

$x = 5; var_dump(max($x, flr_set(100)));
$x = 5; var_dump(in_array($x, [100], flr_set(100)));
$x = 'a'; var_dump(array_key_exists($x, ['b' => 1] + [flr_set('b')]));
$x = 'y'; var_dump(sprintf('%s-%s', $x, flr_set('z')));
$x = 'abc'; var_dump(str_contains($x, (string)flr_set('zz1')));
$x = 'abcdef'; var_dump(substr($x, flr_set('XY')));
unset($u); var_dump(max($u, flr_def()));
$x = 5; var_dump(flr_plain($x, flr_set(100)));
$x = 5; var_dump(abs($x) + flr_set(100));

function flr_local() {
    $v = 1;
    $f = function () use (&$v) { $v = 9; return 2; };
    return max($v, $f());
}
var_dump(flr_local());
}

namespace Flr {
function max($a, $b) { return "own:$a"; }
$x = 5; var_dump(max($x, \flr_set(100)));
$x = 5; var_dump(\max($x, \flr_set(100)));
}
?>
--EXPECT--
int(100)
bool(true)
bool(true)
string(3) "z-1"
bool(true)
string(1) "Y"
int(7)
int(5)
int(6)
int(9)
string(5) "own:5"
int(100)
