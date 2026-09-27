--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Every reserved word may be a named-argument label, like php
--FILE--
<?php
/* A parameter may be called anything (`function f($new, $print)`), so php
 * accepts all 73 reserved words as named-argument labels. Sixteen of them were
 * claimed by their own construct branch in the expression parser here, so
 * `f(new: 1)` and fifteen more were a compile fatal on source php runs. */
function reservedLabelSink(...$a) { return $a; }

var_dump(reservedLabelSink(
    new: 1, clone: 2, print: 3, echo: 4, and: 5, or: 6, xor: 7,
    instanceof: 8, function: 9, match: 10, throw: 11, yield: 12,
    include: 13, include_once: 14, require: 15, require_once: 16,
));

/* the neighbours that also put a ':' in an expression keep working */
const RESERVED_LABEL_K = 'K';
class ReservedLabelHolder { const K = 'CK'; }

function reservedLabelNamed($new = 'dn', $print = 'dp', $and = 'da') { return [$new, $print, $and]; }
var_dump(reservedLabelNamed(and: 'A', new: 'N'));
var_dump(reservedLabelNamed(new: (true ? 'x' : 'y'), print: 0 ?: 'z'));
var_dump(true ? RESERVED_LABEL_K : 'other', false ? 'x' : ReservedLabelHolder::K);

$i = 0;
reservedLabel: $i++;
if ($i < 2) { goto reservedLabel; }
var_dump($i);

switch (2) {
    case 1: echo "one\n"; break;
    case 2: echo "two\n"; break;
    default: echo "d\n";
}
if ($i): echo "alt\n"; else: echo "no\n"; endif;
?>
--EXPECT--
array(16) {
  ["new"]=>
  int(1)
  ["clone"]=>
  int(2)
  ["print"]=>
  int(3)
  ["echo"]=>
  int(4)
  ["and"]=>
  int(5)
  ["or"]=>
  int(6)
  ["xor"]=>
  int(7)
  ["instanceof"]=>
  int(8)
  ["function"]=>
  int(9)
  ["match"]=>
  int(10)
  ["throw"]=>
  int(11)
  ["yield"]=>
  int(12)
  ["include"]=>
  int(13)
  ["include_once"]=>
  int(14)
  ["require"]=>
  int(15)
  ["require_once"]=>
  int(16)
}
array(3) {
  [0]=>
  string(1) "N"
  [1]=>
  string(2) "dp"
  [2]=>
  string(1) "A"
}
array(3) {
  [0]=>
  string(1) "x"
  [1]=>
  string(1) "z"
  [2]=>
  string(2) "da"
}
string(1) "K"
string(2) "CK"
int(2)
two
alt
--CLEAN--
<?php
