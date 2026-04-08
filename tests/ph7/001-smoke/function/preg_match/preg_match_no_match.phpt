--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match returns 0 on no match
--FILE--
<?php
$m = null;
$r = preg_match('/xyz/', 'Hello World', $m);
echo $r . "\n";
echo count($m) . "\n";
?>
--EXPECT--
0
0
--CLEAN--
<?php

