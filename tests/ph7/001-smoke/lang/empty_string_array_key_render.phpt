--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An empty-string array key renders as "" (not a stray space) in var_dump/print_r/var_export
--DESCRIPTION--
SyBlobData() returns NULL for a zero-length blob, and the formatter's legacy "print a single
space for a NULL string pointer" fallback rendered an empty-string array key as " " — visible
as `[" "]` in var_dump, `[ ]` in print_r, and `Undefined array key " "` in the warning. An
explicit precision of 0 (`%.*s` with a 0 length) is now treated as the EMPTY string and emits
nothing; a genuinely NULL string still prints the space. Independent of the null-array-offset
fix, which merely made empty-string keys common enough to expose this.
--FILE--
<?php
$a = ['' => 'v', 'b' => 2];
echo "== var_dump ==\n"; var_dump($a);
echo "== print_r ==\n"; print_r($a);
echo "== var_export ==\n"; var_export($a); echo "\n";
echo "== nested empty key ==\n"; var_dump(['' => ['' => 1]]);
?>
--EXPECT--
== var_dump ==
array(2) {
  [""]=>
  string(1) "v"
  ["b"]=>
  int(2)
}
== print_r ==
Array
(
    [] => v
    [b] => 2
)
== var_export ==
array (
  '' => 'v',
  'b' => 2,
)
== nested empty key ==
array(1) {
  [""]=>
  array(1) {
    [""]=>
    int(1)
  }
}
