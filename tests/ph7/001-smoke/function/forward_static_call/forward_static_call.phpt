--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
forward_static_call forwards call to a static method
--FILE--
<?php
class X { public static function v($x){ return $x*2; } }
class CallProxy{ public static function f(){ return forward_static_call(array('X','v'), 10); } }
echo CallProxy::f() . "\n";
?>
--EXPECT--
20
--CLEAN--
<?php
