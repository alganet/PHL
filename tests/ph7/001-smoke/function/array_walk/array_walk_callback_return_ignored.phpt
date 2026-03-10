--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk ignores callback return value including false
--FILE--
<?php
$a = array('a' => 1, 'b' => 2);
$count = 0;
function walk_return_false($v, $k) {
    global $count;
    $count++;
    return false;
}
$result = array_walk($a, 'walk_return_false');
echo $result ? 'true' : 'false';
echo "\n";
echo $count;
?>
--EXPECT--
true
2
--CLEAN--
<?php
unset($a, $count, $result);
