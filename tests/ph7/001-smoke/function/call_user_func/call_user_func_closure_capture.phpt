--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Closure capture with call_user_func/call_user_func_array
--FILE--
<?php
$prefix = "Value:";
$closure = function($x) use ($prefix) {
  echo $prefix . " " . $x . "\n";
};
call_user_func($closure, "A");
call_user_func_array($closure, array("B"));
?>
--EXPECT--
Value: A
Value: B
--CLEAN--
<?php
unset($prefix, $closure);
