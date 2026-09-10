--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Spread of an empty array into a builtin call (operand-stack balance)
--FILE--
<?php
// An unpack expanding to fewer operands than the compile-time arg count once
// underflowed the operand stack on the builtin-return path.
$empty = [];
var_dump(array_merge(...$empty));
var_dump(array_merge(...[]));
var_dump(max(1, 2, ...$empty));
$parts = [[1, 2], [3]];
var_dump(array_merge(...$parts));
echo implode(",", [...$empty, 'a', ...$empty]), "\n";
// In a loop, to exercise the return-slot repeatedly.
$acc = [];
for ($i = 0; $i < 3; $i++) { $acc = array_merge($acc, ...[]); }
var_dump(count($acc));
?>
--EXPECT--
array(0) {
}
array(0) {
}
int(2)
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
a
int(0)
--CLEAN--
<?php
