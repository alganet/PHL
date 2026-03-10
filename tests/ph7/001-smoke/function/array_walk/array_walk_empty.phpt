--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk on empty array returns true without calling callback
--FILE--
<?php
$a = array();
$called = 0;
function walk_empty_cb($v, $k) {
    global $called;
    $called++;
}
$result = array_walk($a, 'walk_empty_cb');
echo $result ? 'true' : 'false';
echo "\n";
echo $called;
?>
--EXPECT--
true
0
--CLEAN--
<?php
unset($a, $called, $result);
