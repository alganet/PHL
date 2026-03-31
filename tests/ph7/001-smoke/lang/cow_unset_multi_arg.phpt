--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: unset with multiple comma-separated arguments
--FILE--
<?php
$a = 1;
$b = 2;
$c = 3;
unset($a, $c);
echo isset($a) ? "set" : "unset";
echo " ";
echo isset($b) ? "set" : "unset";
echo " ";
echo isset($c) ? "set" : "unset";
echo "\n";
?>
--EXPECT--
unset set unset
--CLEAN--
<?php
unset($b);
