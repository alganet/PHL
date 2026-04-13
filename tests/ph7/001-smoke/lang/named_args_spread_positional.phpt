--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: spread with positional args only (no named mixing)
--FILE--
<?php
function naspf($a, $b, $c) {
    echo "a=$a b=$b c=$c\n";
}
$arr = [1, 2, 3];
naspf(...$arr);

$partial = [10, 20];
naspf(5, ...$partial);
?>
--EXPECT--
a=1 b=2 c=3
a=5 b=10 c=20
--CLEAN--
<?php
