--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
uksort should sort keys using a user callback (compare by length)
--FILE--
<?php
$a = array('aa' => 1, 'b' => 2, 'ccc' => 3);
uksort($a, function($k1, $k2) { return strlen($k1) - strlen($k2); });
foreach($a as $k => $v){ echo $k . ':' . $v . PHP_EOL; }
?>
--EXPECT--
b:2
aa:1
ccc:3
--CLEAN--
<?php
unset($a);
?>
