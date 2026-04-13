--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: ternary expression as argument value
--FILE--
<?php
function natvf($x) { echo "x=$x\n"; }
$a = true;
natvf(x: $a ? "yes" : "no");
natvf(x: !$a ? "yes" : "no");
natvf(x: 1 > 0 ? "gt" : "lt");
?>
--EXPECT--
x=yes
x=no
x=gt
--CLEAN--
<?php
