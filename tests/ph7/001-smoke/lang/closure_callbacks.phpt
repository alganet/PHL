--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Closures work as callbacks for array_map / array_filter / usort / call_user_func / array_reduce
--FILE--
<?php
echo implode(",", array_map(fn($x) => $x * 2, [1,2,3])), "\n";
echo implode(",", array_filter([1,2,3,4], function($x){ return $x % 2 === 0; })), "\n";
$a = [3,1,2]; usort($a, fn($x,$y) => $x - $y); echo implode(",", $a), "\n";
echo call_user_func(fn($x) => $x . "!", "ok"), "\n";
echo array_reduce([1,2,3,4], fn($c,$x) => $c + $x, 0), "\n";
?>
--EXPECT--
2,4,6
2,4
1,2,3
ok!
10
--CLEAN--
<?php
