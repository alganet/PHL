--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A short ternary inside parentheses is an ordinary operand
--FILE--
<?php
/* The elvis node records its operands in pCond/pRight and leaves pLeft NULL,
 * which is also the tree builder's "not linked yet" marker -- so a COMPLETED
 * one sitting in a parenthesised group was re-entered by the ternary pass and
 * refused. Every shape below is valid php that would not compile. */
$v = 7;
$u = null;
$a = ['k' => 0];

/* the group as a whole operand */
$x = (0 ?: 5);
echo $x, "\n";
echo (0 ?: 5), "\n";
print (0 ?: 5);
echo "\n";
echo ((0) ?: 5), "\n";
echo ((0 ?: 5)), "\n";
echo (0 ?: (5)), "\n";
echo (0 ?: 5 ?: 7), "\n";

/* under a unary operator or a cast */
var_dump(!(0 ?: 5));
var_dump(-(0 ?: 5));
var_dump(~(0 ?: 5));
var_dump((int)("" ?: "12abc"));
var_dump(@($u ?: 'd'));

/* as a subscript key, a condition, an argument, an array element */
$m = [(0 ?: 5) => 'five'];
var_dump($m);
if ((0 ?: 5)) { echo "cond\n"; }
var_dump(strlen(('' ?: 'abcd')));
var_dump([($a['k'] ?: 'fallback'), ($v ?: 'unused')]);
?>
--EXPECT--
5
5
5
5
5
5
5
bool(false)
int(-5)
int(-6)
int(12)
string(1) "d"
array(1) {
  [5]=>
  string(4) "five"
}
cond
int(4)
array(2) {
  [0]=>
  string(8) "fallback"
  [1]=>
  int(7)
}
--CLEAN--
<?php
