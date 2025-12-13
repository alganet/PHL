--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
forward_static_call_array forwards call to static method using an array of args
--SKIPIF--
<?php
if (!function_exists('forward_static_call_array')) {
    echo "skip";
}
?>
--FILE--
<?php
class Y { public static function v($a,$b){ return $a+$b; } }
class CallProxy2{ public static function f(){ return forward_static_call_array(array('Y','v'), array(3,7)); } }
echo CallProxy2::f() . "\n";
?>
--EXPECT--
10


