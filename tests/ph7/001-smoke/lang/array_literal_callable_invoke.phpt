--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A literal array callable invoked directly — [obj, 'm']() — calls the method
--DESCRIPTION--
The '(' following a short-array literal was never marked as the call
operator: the literal is a SELF-CONTAINED expression node whose start token
is '[', which both misses ExprVerifyNodes' raw-']' test and carries
PH7_TK_OP (as the subscript operator), tripping the alpha-stream-operator
guard. The '()' parsed as an empty grouping paren and was silently DROPPED —
`$r = [new C, "m"]();` assigned the ARRAY itself, a silent wrong answer.
The variable-held detour (`$f = [new C, "m"]; $f();`) always worked; both
now compile to the same OP_CALL with the array on the stack.
--FILE--
<?php
class AlcInv {
    public function m($x = 0) { return "m" . $x; }
    public static function s($x) { return $x * 2; }
}
$alc_o = new AlcInv;
var_dump([new AlcInv, "m"]());
var_dump([new AlcInv, "m"](7));
var_dump([AlcInv::class, "s"](21));
var_dump(["AlcInv", "s"](5));
var_dump([$alc_o, "m"](1) . "-chained");
try { [1, 2, 3](); } catch (Error $e) { echo "E1:", $e->getMessage(), "\n"; }
try { [new AlcInv, "nom"](); } catch (Error $e) { echo "E2:", $e->getMessage(), "\n"; }
$alc_nested = [[$alc_o, "m"]];
var_dump($alc_nested[0](3));
var_dump([1, 2, 3][1]);
$alc_grp = ([1, 2]);
var_dump(count($alc_grp));
var_dump(in_array(2, [1, 2]) ? ("y") : "n");
foreach ([3, 4] as $alc_v) { echo $alc_v; }
echo "\n";
var_dump([1, 2] + [3, 4, 5]);
echo "end\n";
?>
--EXPECT--
string(2) "m0"
string(2) "m7"
int(42)
int(10)
string(10) "m1-chained"
E1:Array callback must have exactly two elements
E2:Call to undefined method AlcInv::nom()
string(2) "m3"
int(2)
int(2)
string(1) "y"
34
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(5)
}
end
--CLEAN--
<?php
unset($alc_o, $alc_nested, $alc_grp, $alc_v);
