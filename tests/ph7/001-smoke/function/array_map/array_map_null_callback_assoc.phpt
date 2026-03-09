--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_map with null callback preserves associative keys
--FILE--
<?php
$r = array_map(null, array('a' => 1, 'b' => 2));
echo $r['a'] . PHP_EOL;
echo $r['b'] . PHP_EOL;
?>
--EXPECT--
1
2
--CLEAN--
<?php
unset($r);
