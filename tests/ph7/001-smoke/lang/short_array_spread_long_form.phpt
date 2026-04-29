--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array() long form: spread (...) supported on the same path as short syntax
--FILE--
<?php
$a = ["a" => 1, "b" => 2];
$b = array("c" => 3, ...$a);
echo count($b), "\n";
foreach ($b as $k => $v) {
    echo $k, "=", $v, "\n";
}

$c = array(1, ...array(2, 3), 4);
foreach ($c as $k => $v) {
    echo $k, "=", $v, "\n";
}
?>
--EXPECT--
3
c=3
a=1
b=2
0=1
1=2
2=3
3=4
--CLEAN--
<?php
unset($a, $b, $c);
