--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
spl_autoload_call runs every registered autoloader, empty name included
--FILE--
<?php
$splCallSeen = [];
$splCallA = function ($n) use (&$splCallSeen) { $splCallSeen[] = "A:$n"; };
$splCallB = function ($n) use (&$splCallSeen) { $splCallSeen[] = "B:$n"; };
spl_autoload_register($splCallA);
spl_autoload_register($splCallB);
var_dump(spl_autoload_call('Nope\\Missing'));
spl_autoload_call('');
spl_autoload_call('ArrayObject');
print_r($splCallSeen);
spl_autoload_unregister($splCallA);
spl_autoload_unregister($splCallB);
?>
--EXPECT--
NULL
Array
(
    [0] => A:Nope\Missing
    [1] => B:Nope\Missing
    [2] => A:
    [3] => B:
    [4] => A:ArrayObject
)
--CLEAN--
<?php
unset($splCallSeen, $splCallA, $splCallB);
