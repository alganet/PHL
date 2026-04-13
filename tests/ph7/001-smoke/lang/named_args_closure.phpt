--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: closures and anonymous functions
--FILE--
<?php
$nacf = function($a, $b) {
    echo "a=$a b=$b\n";
};
$nacf(b: "world", a: "hello");
$nacf(a: "X", b: "Y");
?>
--EXPECT--
a=hello b=world
a=X b=Y
--CLEAN--
<?php
