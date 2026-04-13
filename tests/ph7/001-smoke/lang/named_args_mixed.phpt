--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: mixed positional and named
--FILE--
<?php
function namf($a, $b, $c) {
    echo "a=$a b=$b c=$c\n";
}
namf(1, c: 3, b: 2);
namf(1, 2, c: 3);
?>
--EXPECT--
a=1 b=2 c=3
a=1 b=2 c=3
--CLEAN--
<?php
