--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: call_user_func with reference parameter
--FILE--
<?php
function ref_incr(&$x){ $x += 5; }
$a = 3;
call_user_func('ref_incr', $a);
echo $a . "\n";

$b = 10;
call_user_func_array('ref_incr', array(&$b));
echo $b . "\n";
?>
--EXPECTF--
%Aref_incr(): Argument #1 ($x) must be passed by reference, value given%A3%A15%A
--CLEAN--
<?php
unset($a, $b);
