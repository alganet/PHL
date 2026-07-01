--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_replace with array subjects and array patterns (PHP-exact)
--FILE--
<?php
function d($x){ echo json_encode($x), "\n"; }
// array subject -> array result
d(preg_replace('/\d/', 'X', ['a1','b2','c3']));
// array subject preserves keys (string + int)
d(preg_replace('/\d/', '#', ['x'=>'a1','y'=>'b2', 7=>'c3']));
// int/string mixed keys kept in order
d(preg_replace('/o/', '0', [3=>'foo', 1=>'boo', 'k'=>'zoo']));
// array pattern, array replacement (parallel), 3rd replacement missing -> ""
d(preg_replace(['/a/','/b/','/c/'], ['X','Y'], 'abc'));
// array pattern, scalar replacement -> used for every pattern
d(preg_replace(['/a/','/b/'], 'Z', 'abc'));
// array pattern + array subject: each subject gets all patterns
d(preg_replace(['/a/','/b/'], ['1','2'], ['aXb','bYa']));
// empty pattern array -> subject unchanged
d(preg_replace([], 'X', 'abc'));
// replacement longer than patterns -> extras ignored
d(preg_replace(['/a/'], ['X','Y'], 'abc'));
// flags survive with an array subject
d(preg_replace('/a/i', 'X', ['A','bA']));
// backreferences still expand
echo preg_replace('/(\w)(\w)/', '$2$1', 'abcd'), "\n";
// empty subject array
d(preg_replace('/x/', 'Y', []));
// $count accumulates across an array subject
preg_replace('/\d/', '#', ['a1b2','c3'], -1, $cnt);
echo "count=$cnt\n";
// scalar control unchanged
echo preg_replace('/a/', 'X', 'abrakadabra'), "\n";
?>
--EXPECT--
["aX","bX","cX"]
{"x":"a#","y":"b#","7":"c#"}
{"3":"f00","1":"b00","k":"z00"}
"XY"
"ZZc"
["1X2","2Y1"]
"abc"
"Xbc"
["X","bX"]
badc
[]
count=3
XbrXkXdXbrX
--CLEAN--
<?php
