--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: basic out-of-order calling
--FILE--
<?php
function nabf($a, $b, $c) {
    echo "a=$a b=$b c=$c\n";
}
nabf(c: 3, a: 1, b: 2);
nabf(b: 20, c: 30, a: 10);
?>
--EXPECT--
a=1 b=2 c=3
a=10 b=20 c=30
--CLEAN--
<?php
