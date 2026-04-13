--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: nested function calls with named args
--FILE--
<?php
function nanci($x) { return $x * 2; }
function nancp($a, $b) { echo "a=$a b=$b\n"; }
nancp(b: nanci(x: 5), a: nanci(x: 3));
nancp(a: nanci(x: 10), b: nanci(x: 20));
?>
--EXPECT--
a=6 b=10
a=20 b=40
--CLEAN--
<?php
