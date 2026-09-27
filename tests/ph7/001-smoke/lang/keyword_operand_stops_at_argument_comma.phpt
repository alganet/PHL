--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A yield/print operand ends at the argument comma, like php's expr
--FILE--
<?php
/* php's grammar hands `yield`, `yield from` and the one-operand language
 * constructs an `expr`, and a top-level COMMA is not part of an `expr` -- so in
 * an ARGUMENT LIST that comma is the separator. The operand here ran past it
 * and the leftover `, 2` came back as `syntax error, unexpected token ","` on
 * source php runs. An ARRAY literal never showed it: its body is re-split on
 * commas before these nodes are ever extracted. */
function keywordOperandSink($a, $b = 'none') { return [$a, $b]; }

function keywordOperandGen()
{
    var_dump(keywordOperandSink(yield 1, 2));
    var_dump(keywordOperandSink(yield, 3));
    var_dump(keywordOperandSink(yield 4, yield 5));
    var_dump(keywordOperandSink(yield from [6, 7], 8));
}

$it = keywordOperandGen();
for ($n = 0; $n < 6; $n++) {
    echo 'y ', var_export($it->key(), true), ' => ', var_export($it->current(), true), "\n";
    $it->send('S');
}

/* the same rule for a language construct, in a call and in an array */
var_dump(keywordOperandSink(print "p\n", 2));
var_dump([print "q\n", 4]);
?>
--EXPECT--
y 0 => 1
array(2) {
  [0]=>
  string(1) "S"
  [1]=>
  int(2)
}
y 1 => NULL
array(2) {
  [0]=>
  string(1) "S"
  [1]=>
  int(3)
}
y 2 => 4
y 3 => 5
array(2) {
  [0]=>
  string(1) "S"
  [1]=>
  string(1) "S"
}
y 0 => 6
y 1 => 7
array(2) {
  [0]=>
  NULL
  [1]=>
  int(8)
}
p
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(2)
}
q
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(4)
}
--CLEAN--
<?php
