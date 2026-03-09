--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_replace adds keys from replacement that do not exist in original
--FILE--
<?php
$a = array('x' => 1);
$b = array('y' => 2);
$r = array_replace($a, $b);
echo count($r) . "\n";
echo $r['y'];
?>
--EXPECT--
2
2
--CLEAN--
<?php
unset($a, $b, $r);
