--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count_chars() implements all five modes and rejects any other $mode
--FILE--
<?php
// Modes 1 and 3 report the bytes that WERE used; 2 and 4 the ones that were
// NOT. PH7 implemented 0/1/3 and let 2 and 4 fall through to mode 0's full
// 256-entry table -- the exact complement of what was asked for, silently.
var_dump(count_chars("hello", 1));
var_dump(count_chars("hello", 3));

$all  = count_chars("hello", 0);
$used = count_chars("hello", 1);
$un   = count_chars("hello", 2);
var_dump(count($all), count($used), count($un));
var_dump($un === array_diff_key($all, $used));
var_dump(count_chars("hello", 4) === implode("", array_map("chr", array_keys($un))));
var_dump(strlen(count_chars("hello", 4)));

// The empty string uses nothing, so mode 4 is every byte and mode 2 the whole table.
var_dump(count_chars("", 1), count_chars("", 3), count(count_chars("", 2)), strlen(count_chars("", 4)));

foreach ([-1, 5, PHP_INT_MAX] as $mode) {
    try {
        count_chars("abc", $mode);
    } catch (ValueError $e) {
        echo $e->getMessage(), "\n";
    }
}
// A $mode php can never coerce to int is a TypeError, not mode 0.
foreach ([["x"], [[]], [new stdClass]] as $case) {
    try {
        count_chars("abc", $case[0]);
    } catch (TypeError $e) {
        echo $e->getMessage(), "\n";
    }
}
// The ones php DOES coerce keep working.
var_dump(count_chars("abc", "2") === count_chars("abc", 2));
var_dump(count_chars("abc", true) === count_chars("abc", 1));
?>
--EXPECT--
array(4) {
  [101]=>
  int(1)
  [104]=>
  int(1)
  [108]=>
  int(2)
  [111]=>
  int(1)
}
string(4) "ehlo"
int(256)
int(4)
int(252)
bool(true)
bool(true)
int(252)
array(0) {
}
string(0) ""
int(256)
int(256)
count_chars(): Argument #2 ($mode) must be between 0 and 4 (inclusive)
count_chars(): Argument #2 ($mode) must be between 0 and 4 (inclusive)
count_chars(): Argument #2 ($mode) must be between 0 and 4 (inclusive)
count_chars(): Argument #2 ($mode) must be of type int, string given
count_chars(): Argument #2 ($mode) must be of type int, array given
count_chars(): Argument #2 ($mode) must be of type int, stdClass given
bool(true)
bool(true)
--CLEAN--
<?php
