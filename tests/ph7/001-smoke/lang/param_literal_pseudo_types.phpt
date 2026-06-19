--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
true/iterable parameter types are enforced by value (PHP 8.2 / 7.4)
--FILE--
<?php
function plpTrue(true $x) { return $x; }
function plpIter(iterable $x) { return is_array($x) ? "arr" : "trav"; }
echo plpTrue(true) === true ? "true_ok\n" : "true_fail\n";
try { plpTrue(false); } catch (TypeError $e) { echo "false_rejected\n"; }
echo plpIter([1]), "\n";
try { plpIter(5); } catch (TypeError $e) { echo "int_rejected\n"; }
?>
--EXPECT--
true_ok
false_rejected
arr
int_rejected
--CLEAN--
<?php
