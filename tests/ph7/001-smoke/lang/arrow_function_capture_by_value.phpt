--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: capture is by value, later mutations don't leak
--FILE--
<?php
$n = 1;
$f = fn() => $n;
$n = 99;
echo $f(), "\n";
?>
--EXPECT--
1
--CLEAN--
<?php
unset($f, $n);
