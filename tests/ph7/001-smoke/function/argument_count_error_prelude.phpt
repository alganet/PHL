--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Prelude-implemented builtins throw ArgumentCountError on too few arguments
--FILE--
<?php
// Stage 1's aBuiltinArity[] stamps a minimum onto HOST (C) builtins only, so a
// builtin written as embedded PHP in the prelude ran with its parameters unset:
// count_chars() warned "Undefined variable $string" and answered a table,
// ip2long()/class_uses() answered false, preg_filter() reached preg_replace()
// with nulls. php throws ArgumentCountError before the body runs, in its ZPP
// wording ("exactly N" with no optional parameter, "at least N" with one).
$cases = [
    ['count_chars', []],
    ['ip2long', []],
    ['long2ip', []],
    ['class_uses', []],
    ['class_implements', []],
    ['array_change_key_case', []],
    ['array_count_values', []],
    ['array_replace_recursive', []],
    ['checkdate', [1, 2]],
    ['fdiv', [1]],
    ['is_nan', []],
    ['hex2bin', []],
    ['extension_loaded', []],
    ['number_format', []],
    ['http_build_query', []],
    ['preg_filter', ['/a/', 'b']],
    ['preg_grep', ['/a/']],
    ['preg_replace_callback_array', [[]]],
    ['glob', []],
    ['scandir', []],
    ['dir', []],
    ['fileowner', []],
    ['fileperms', []],
    ['tempnam', ['/tmp']],
    ['version_compare', ['1']],
    ['key_exists', ['k']],
    // The three that hand-wrote their own check keep php's exact wording now
    // that the central one owns it.
    ['max', []],
    ['min', []],
    ['array_unshift', []],
];
foreach ($cases as [$fn, $args]) {
    try {
        @call_user_func_array($fn, $args);
        echo "$fn: NO THROW\n";
    } catch (ArgumentCountError $e) {
        echo $e->getMessage(), "\n";
    }
}

// It is php's catchable ArgumentCountError, which extends TypeError.
try {
    count_chars();
} catch (TypeError $e) {
    echo get_class($e), "\n";
}
// A call that DOES satisfy the minimum still runs.
var_dump(count_chars("aab", 3), ip2long("127.0.0.1"));
?>
--EXPECT--
count_chars() expects at least 1 argument, 0 given
ip2long() expects exactly 1 argument, 0 given
long2ip() expects exactly 1 argument, 0 given
class_uses() expects at least 1 argument, 0 given
class_implements() expects at least 1 argument, 0 given
array_change_key_case() expects at least 1 argument, 0 given
array_count_values() expects exactly 1 argument, 0 given
array_replace_recursive() expects at least 1 argument, 0 given
checkdate() expects exactly 3 arguments, 2 given
fdiv() expects exactly 2 arguments, 1 given
is_nan() expects exactly 1 argument, 0 given
hex2bin() expects exactly 1 argument, 0 given
extension_loaded() expects exactly 1 argument, 0 given
number_format() expects at least 1 argument, 0 given
http_build_query() expects at least 1 argument, 0 given
preg_filter() expects at least 3 arguments, 2 given
preg_grep() expects at least 2 arguments, 1 given
preg_replace_callback_array() expects at least 2 arguments, 1 given
glob() expects at least 1 argument, 0 given
scandir() expects at least 1 argument, 0 given
dir() expects at least 1 argument, 0 given
fileowner() expects exactly 1 argument, 0 given
fileperms() expects exactly 1 argument, 0 given
tempnam() expects exactly 2 arguments, 1 given
version_compare() expects at least 2 arguments, 1 given
key_exists() expects exactly 2 arguments, 1 given
max() expects at least 1 argument, 0 given
min() expects at least 1 argument, 0 given
array_unshift() expects at least 1 argument, 0 given
ArgumentCountError
string(2) "ab"
int(2130706433)
--CLEAN--
<?php
