--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array syntax: spread with colliding string keys (later wins)
--FILE--
<?php
$a = ["a" => 1, ...["a" => 2]];
echo count($a), "\n";
echo "a=", $a["a"], "\n";

$b = [...["k" => 1], "k" => 2];
echo "k=", $b["k"], "\n";

$c = ["k" => 1, ...["k" => 2], "k" => 3];
echo "k=", $c["k"], "\n";
?>
--EXPECT--
1
a=2
k=2
k=3
--CLEAN--
<?php
unset($a, $b, $c);
