--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An unordered comparison answers 1 from either side
--FILE--
<?php
/* php answers an UNORDERED comparison with 1 whichever way round it is asked,
 * which is what leaves ==, <, >, <= and >= all false at once while <=> is 1
 * from both sides. Two kinds of pair are unordered: a NaN against anything php
 * reads as a NUMBER or a STRING, and two same-sized arrays neither of which
 * contains the other.
 *
 * PHL got both wrong, in two different places. The NaN screen ran AFTER the
 * comparator, which converts its operands in place -- so a NaN compared with a
 * string had already been rendered as the bytes "NAN" and the screen saw two
 * strings: `NAN == "NAN"` was true. And `>`/`>=` were read off the sign of the
 * comparison from THIS side instead of asking it from the other, so every
 * unordered pair was "greater". */
$n = NAN;
$rows = [
    'NAN vs 1.5'    => [$n, 1.5],
    '1.5 vs NAN'    => [1.5, $n],
    'NAN vs NAN'    => [$n, $n],
    'NAN vs 3'      => [$n, 3],
    'NAN vs 0'      => [$n, 0],
    'NAN vs "NAN"'  => [$n, "NAN"],
    '"NAN" vs NAN'  => ["NAN", $n],
    'NAN vs "1.5"'  => [$n, "1.5"],
    'NAN vs "abc"'  => [$n, "abc"],
    'NAN vs ""'     => [$n, ""],
    'NAN vs true'   => [$n, true],
    'NAN vs false'  => [$n, false],
    'NAN vs null'   => [$n, null],
    'NAN vs []'     => [$n, []],
    'NAN vs [1]'    => [$n, [1]],
    'INF vs 1.5'    => [INF, 1.5],
    '[1] vs ["a"=>1]' => [[1], ["a" => 1]],
    '["a"=>1] vs [1]' => [["a" => 1], [1]],
    '[1] vs [1,2]'  => [[1], [1, 2]],
    '[1] vs [1]'    => [[1], [1]],
];
foreach ($rows as $label => [$a, $b]) {
    printf("%-16s == %-5s != %-5s < %-5s > %-5s <= %-5s >= %-5s <=> %2d === %-5s\n",
        $label, var_export($a == $b, true), var_export($a != $b, true),
        var_export($a < $b, true), var_export($a > $b, true),
        var_export($a <= $b, true), var_export($a >= $b, true),
        $a <=> $b, var_export($a === $b, true));
}
/* Every other place that ORDERS or MATCHES values goes through the same
 * comparator, with no operator of its own: a NaN is found by none of them. */
var_dump(in_array($n, ["NAN"]), in_array($n, [$n]), in_array($n, [$n], true),
         array_search("NAN", [$n]), array_keys([$n, 1.5], $n));
var_dump(max($n, 3), min($n, 3), max(3, $n), min(3, $n));
var_dump((function ($x) { switch ($x) { case "NAN": return 'string'; case NAN: return 'nan'; }
                          return 'none'; })($n));
/* A NaN is still a float everywhere that is not a comparison. */
var_dump($n + 1, is_nan($n), is_float($n), (string)(fdiv(0, 0) <=> 0));
?>
--EXPECT--
NAN vs 1.5       == false != true  < false > false <= false >= false <=>  1 === false
1.5 vs NAN       == false != true  < false > false <= false >= false <=>  1 === false
NAN vs NAN       == false != true  < false > false <= false >= false <=>  1 === false
NAN vs 3         == false != true  < false > false <= false >= false <=>  1 === false
NAN vs 0         == false != true  < false > false <= false >= false <=>  1 === false
NAN vs "NAN"     == false != true  < false > false <= false >= false <=>  1 === false
"NAN" vs NAN     == false != true  < false > false <= false >= false <=>  1 === false
NAN vs "1.5"     == false != true  < false > false <= false >= false <=>  1 === false
NAN vs "abc"     == false != true  < false > false <= false >= false <=>  1 === false
NAN vs ""        == false != true  < false > false <= false >= false <=>  1 === false
NAN vs true      == true  != false < false > false <= true  >= true  <=>  0 === false
NAN vs false     == false != true  < false > true  <= false >= true  <=>  1 === false
NAN vs null      == false != true  < false > true  <= false >= true  <=>  1 === false
NAN vs []        == false != true  < true  > false <= true  >= false <=> -1 === false
NAN vs [1]       == false != true  < true  > false <= true  >= false <=> -1 === false
INF vs 1.5       == false != true  < false > true  <= false >= true  <=>  1 === false
[1] vs ["a"=>1]  == false != true  < false > false <= false >= false <=>  1 === false
["a"=>1] vs [1]  == false != true  < false > false <= false >= false <=>  1 === false
[1] vs [1,2]     == false != true  < true  > false <= true  >= false <=> -1 === false
[1] vs [1]       == true  != false < false > false <= true  >= true  <=>  0 === true 
bool(false)
bool(false)
bool(false)
bool(false)
array(0) {
}
int(3)
int(3)
float(NAN)
float(NAN)
string(4) "none"
float(NAN)
bool(true)
bool(true)
string(1) "1"
