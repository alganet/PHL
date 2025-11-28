--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
call_user_func with object method callback
--FILE--
<?php
class MyClass {
    public function inc($v) { return $v + 1; }
}
$obj = new MyClass();
echo call_user_func(array($obj, 'inc'), 4) . "\n";

class StaticLike {
    public static function add($a, $b) { return $a + $b; }
}
echo call_user_func(array('StaticLike', 'add'), 2, 3) . "\n";
?>
--EXPECT--
5
5

--CLEAN--
<?php
unset($obj);
?>
