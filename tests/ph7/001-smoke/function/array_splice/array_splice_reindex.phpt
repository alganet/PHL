--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: array_splice renumbers integer keys like php
--FILE--
<?php
$a = ["a", "c"];
array_splice($a, 1, 0, ["b"]);
print_r($a);
$b = ["x", "y", "z"];
$r = array_splice($b, 0, 1);
print_r($b);
print_r($r);
$c = ["a", "c", "x" => 9, "d"];
array_splice($c, 1, 1, ["b1", "b2"]);
print_r($c);
$e = [5 => 1];
array_splice($e, 0, 0);
print_r($e);
$f = [1, 2, 3, 4];
print_r(array_splice($f, -2, 1));
print_r($f);
?>
--EXPECT--
Array
(
    [0] => a
    [1] => b
    [2] => c
)
Array
(
    [0] => y
    [1] => z
)
Array
(
    [0] => x
)
Array
(
    [0] => a
    [1] => b1
    [2] => b2
    [x] => 9
    [3] => d
)
Array
(
    [0] => 1
)
Array
(
    [0] => 3
)
Array
(
    [0] => 1
    [1] => 2
    [2] => 4
)
--CLEAN--
<?php
unset($a, $b, $c, $e, $f, $r);
