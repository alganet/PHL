--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Prelude-implemented builtins throw ArgumentCountError on too many arguments
--FILE--
<?php
// The mirror of the too-FEW gap: php rejects a builtin call carrying more
// arguments than the signature declares, PHL accepted the extras silently
// (count_chars("a", 1, 2) just answered mode 1). php says "exactly N" when the
// bounds coincide and "at most N" when there are optional parameters.
$cases = [
    ['count_chars', ['a', 1, 2]],
    ['array_count_values', [[], 1]],
    ['date_get_last_errors', [1]],
    ['ip2long', ['1.2.3.4', 2]],
    ['long2ip', [1, 2]],
    ['fdiv', [1, 2, 3]],
    ['is_nan', [1, 2]],
    ['checkdate', [1, 1, 2000, 4]],
    ['class_uses', ['stdClass', true, 3]],
    ['glob', ['*', 0, 3]],
    ['scandir', ['.', 0, null, 4]],
    ['dir', ['.', null, 3]],
    ['tempnam', ['/tmp', 'x', 3]],
    ['hex2bin', ['41', 2]],
    ['number_format', [1, 2, '.', ',', 5]],
    ['preg_grep', ['/a/', [], 0, 4]],
    ['version_compare', ['1', '2', 'lt', 4]],
    ['key_exists', ['k', [], 3]],
    ['clearstatcache', [true, '', 3]],
];
foreach ($cases as [$fn, $args]) {
    try {
        @call_user_func_array($fn, $args);
        echo "$fn: NO THROW\n";
    } catch (ArgumentCountError $e) {
        echo $e->getMessage(), "\n";
    }
}

// A VARIADIC signature has no maximum — these keep taking everything.
var_dump(max(1, 2, 3, 4, 5, 6));
var_dump(min(1, 2, 3, 4, 5, 6));
var_dump(array_merge_recursive([1], [2], [3], [4]));
$a = [9];
array_unshift($a, 1, 2, 3, 4);
var_dump($a);
?>
--EXPECT--
count_chars() expects at most 2 arguments, 3 given
array_count_values() expects exactly 1 argument, 2 given
date_get_last_errors() expects exactly 0 arguments, 1 given
ip2long() expects exactly 1 argument, 2 given
long2ip() expects exactly 1 argument, 2 given
fdiv() expects exactly 2 arguments, 3 given
is_nan() expects exactly 1 argument, 2 given
checkdate() expects exactly 3 arguments, 4 given
class_uses() expects at most 2 arguments, 3 given
glob() expects at most 2 arguments, 3 given
scandir() expects at most 3 arguments, 4 given
dir() expects at most 2 arguments, 3 given
tempnam() expects exactly 2 arguments, 3 given
hex2bin() expects exactly 1 argument, 2 given
number_format() expects at most 4 arguments, 5 given
preg_grep() expects at most 3 arguments, 4 given
version_compare() expects at most 3 arguments, 4 given
key_exists() expects exactly 2 arguments, 3 given
clearstatcache() expects at most 2 arguments, 3 given
int(6)
int(1)
array(4) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
  [3]=>
  int(4)
}
array(5) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
  [3]=>
  int(4)
  [4]=>
  int(9)
}
--CLEAN--
<?php
