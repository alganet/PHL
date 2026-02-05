--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Nested function linking to upper frame variables via 'global'
--FILE--
<?php
$a = 1;
function f() {
    global $a;
    echo $a . "\n"; // 1
    $a++;
    function g(){ global $a; echo $a . "\n"; $a++; }
    g();
}
f();
echo $a . "\n"; // 3
?>
--EXPECT--
1
2
3
--CLEAN--
<?php
unset($a);
