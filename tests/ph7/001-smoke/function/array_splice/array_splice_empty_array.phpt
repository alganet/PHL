--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_splice on empty array returns empty result
--FILE--
<?php
$a = array();
$r = array_splice($a, 0);
echo count($r) . "\n";
echo count($a);
?>
--EXPECT--
0
0
--CLEAN--
<?php
unset($a, $r);
