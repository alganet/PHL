--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Arrow function: default parameter value
--FILE--
<?php
$f = fn($x = 10) => $x * 2;
echo $f(), "\n";
echo $f(5), "\n";
?>
--EXPECT--
20
10
--CLEAN--
<?php
unset($f);
