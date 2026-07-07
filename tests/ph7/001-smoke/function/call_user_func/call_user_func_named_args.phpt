--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
call_user_func forwards name: arguments to the callback
--FILE--
<?php
function cufNamedF($a, $b) { echo "$a/$b\n"; }
call_user_func('cufNamedF', 1, b: 9);       // positional + named
call_user_func('cufNamedF', b: 9, a: 1);    // both named, out of order
call_user_func('cufNamedF', a: 1, b: 2);    // both named, in order
call_user_func('cufNamedF', 5, 6);          // plain positional still works

$cufNamedClo = function ($x, $y, $z) { echo "$x-$y-$z\n"; };
call_user_func($cufNamedClo, z: 3, x: 1, y: 2);

class CufNamedC {
    public function m($a, $b) { echo "m:$a/$b\n"; }
    public static function s($a, $b) { echo "s:$a/$b\n"; }
}
$cufNamedObj = new CufNamedC();
call_user_func([$cufNamedObj, 'm'], b: 9, a: 1);
call_user_func(['CufNamedC', 's'], b: 9, a: 1);

function cufNamedCollect(...$args) { echo implode(',', array_keys($args)) . "\n"; }
call_user_func('cufNamedCollect', x: 1, y: 2);
?>
--EXPECT--
1/9
1/9
1/2
5/6
1-2-3
m:1/9
s:1/9
x,y
--CLEAN--
<?php

