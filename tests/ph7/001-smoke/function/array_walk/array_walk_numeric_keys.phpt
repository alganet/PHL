--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk works with numeric (integer) keys
--FILE--
<?php
$a = array(10, 20, 30);
function walk_numkeys($v, $k) {
    echo $k . ':' . $v . "\n";
}
array_walk($a, 'walk_numkeys');
?>
--EXPECT--
0:10
1:20
2:30
--CLEAN--
<?php
unset($a);
