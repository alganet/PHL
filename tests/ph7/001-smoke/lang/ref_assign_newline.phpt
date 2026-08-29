--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Reference assignment with newline between = and &

--FILE--
<?php
$a = 10;
$b =
&$a;
$a = 20;
if ($b === 20) {
    echo "Reference with newline ok\n";
}

$x = "hello";
$y =
    &$x;
$x = "world";
if ($y === "world") {
    echo "Reference with spaces and newline ok\n";
}
?>
--EXPECT--
Reference with newline ok
Reference with spaces and newline ok
--CLEAN--
<?php
unset($a, $b, $x, $y);
