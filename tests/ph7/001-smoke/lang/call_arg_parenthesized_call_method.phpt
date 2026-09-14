--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A (call(...))->method() expression works as a function-call argument
--FILE--
<?php
class CapmD {
    public static function make($a, $b) { return new CapmD; }
    public function s() { return "ok"; }
}
function mk() { return new CapmD; }
function capm_g($x, $y) { return "$x|$y"; }
function capm_h($x) { return $x; }
function three($a, $b, $c) { return "$a,$b,$c"; }

echo capm_h((CapmD::make(1, 2))->s()), "\n";              // single arg
echo capm_g("e", (CapmD::make(1, 2))->s()), "\n";         // as 2nd arg
echo capm_g((CapmD::make(1, 2))->s(), "y"), "\n";         // as 1st arg
echo capm_g("e", (mk())->s(),), "\n";                 // trailing comma
echo three("a", (CapmD::make(1, 2))->s(), "c"), "\n";// middle of three
echo capm_h(capm_g((mk())->s(), "z")), "\n";               // nested
print_r(array_merge([1], [2]));                  // short-array args still split
?>
--EXPECT--
ok
e|ok
ok|y
e|ok
a,ok,c
ok|z
Array
(
    [0] => 1
    [1] => 2
)
--CLEAN--
<?php
