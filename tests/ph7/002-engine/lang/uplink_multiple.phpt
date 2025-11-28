--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
UPLINK: Link multiple variables to upper frame using 'global'
--FILE--
<?php
$a = 1;
$b = 2;
function varf() {
    global $a, $b;
    echo $a . "\n"; // 1
    echo $b . "\n"; // 2
    $a = 10;
    $b = 20;
}
varf();
echo $a . "\n"; // 10
echo $b . "\n"; // 20
?>
--EXPECT--
1
2
10
20

--CLEAN--
<?php
unset($a, $b);
?>
