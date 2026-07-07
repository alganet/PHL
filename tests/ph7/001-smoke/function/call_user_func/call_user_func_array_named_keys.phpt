--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
call_user_func_array maps string array keys to named arguments
--FILE--
<?php
function cufaKeysF($a, $b) { echo "$a/$b\n"; }
call_user_func_array('cufaKeysF', ['b' => 9, 'a' => 1]);  // both named
call_user_func_array('cufaKeysF', [1, 'b' => 9]);         // positional + named
call_user_func_array('cufaKeysF', [1, 2]);                // plain positional still works

$cufaKeysClo = function ($x, $y, $z) { echo "$x-$y-$z\n"; };
call_user_func_array($cufaKeysClo, ['z' => 3, 'x' => 1, 'y' => 2]);

function cufaKeysCollect($first, ...$rest) {
    echo $first . ':' . implode(',', array_keys($rest)) . "\n";
}
call_user_func_array('cufaKeysCollect', ['first' => 'F', 'k' => 'v', 'm' => 'w']);
?>
--EXPECT--
1/9
1/9
1/2
1-2-3
F:k,m
--CLEAN--
<?php

