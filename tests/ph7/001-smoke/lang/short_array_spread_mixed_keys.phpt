--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array syntax: spread (...) of array with mixed int/string keys
--FILE--
<?php
$a = ["a" => 1, 0 => "x", "b" => 2];
$b = [...$a];
echo count($b), "\n";
foreach ($b as $k => $v) {
    echo $k, "=", $v, "\n";
}
?>
--EXPECT--
3
a=1
0=x
b=2
--CLEAN--
<?php
unset($a, $b);
