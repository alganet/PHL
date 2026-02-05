--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Unary +/- operator disambiguation after increment/decrement
--FILE--
<?php
// Test that +/- after ++ is treated as binary operator
$a = 5;
$result = $a++ -2;
if ($result === 3) {
    echo "post-increment minus OK\n";
} else {
    echo "post-increment minus FAIL: $result\n";
}

$b = 5;
$result = $b++ +2;
if ($result === 7) {
    echo "post-increment plus OK\n";
} else {
    echo "post-increment plus FAIL: $result\n";
}

$c = 5;
$result = $c-- -2;
if ($result === 3) {
    echo "post-decrement minus OK\n";
} else {
    echo "post-decrement minus FAIL: $result\n";
}

$d = 5;
$result = $d-- +2;
if ($result === 7) {
    echo "post-decrement plus OK\n";
} else {
    echo "post-decrement plus FAIL: $result\n";
}
?>
--EXPECT--
post-increment minus OK
post-increment plus OK
post-decrement minus OK
post-decrement plus OK
--CLEAN--
<?php
unset($a, $result, $b, $c, $d);
