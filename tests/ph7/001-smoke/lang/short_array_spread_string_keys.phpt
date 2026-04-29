--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array syntax: spread (...) preserves string keys (PHP 8.1)
--FILE--
<?php
$a = ["a" => 1, "b" => 2];
$b = ["c" => 3, ...$a];
echo count($b), "\n";
foreach ($b as $k => $v) {
    echo $k, "=", $v, "\n";
}
?>
--EXPECT--
3
c=3
a=1
b=2
--CLEAN--
<?php
unset($a, $b);
