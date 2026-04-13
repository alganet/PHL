--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: trailing comma
--FILE--
<?php
function natcf($x, $y) { echo "x=$x y=$y\n"; }
natcf(x: 1, y: 2,);
natcf(y: 20, x: 10,);
?>
--EXPECT--
x=1 y=2
x=10 y=20
--CLEAN--
<?php
