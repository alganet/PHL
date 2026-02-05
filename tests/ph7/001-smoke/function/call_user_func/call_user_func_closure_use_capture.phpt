--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Closure capturing by value and invoked via call_user_func
--FILE--
<?php
$cnt = 1;
$closure = function() use ($cnt) {
    return $cnt + 1;
};
echo $closure() . "\n";
echo call_user_func($closure) . "\n";
--EXPECT--
2
2
--CLEAN--
<?php
unset($cnt, $closure);
