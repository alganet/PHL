--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
iterable property type accepts array and Traversable, rejects other types (PHP 7.4)
--FILE--
<?php
class IterablePropHolder { public iterable $x = []; }
$ip = new IterablePropHolder();
$ip->x = [1, 2];
echo is_array($ip->x) ? "arr_ok\n" : "arr_fail\n";
$ip->x = (function () { yield 1; })();   // a Generator is Traversable
echo ($ip->x instanceof Traversable) ? "trav_ok\n" : "trav_fail\n";
try { $ip->x = 5; } catch (TypeError $e) { echo "int_rejected\n"; }
?>
--EXPECT--
arr_ok
trav_ok
int_rejected
--CLEAN--
<?php
unset($ip);
