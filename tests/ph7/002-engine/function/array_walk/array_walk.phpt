--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_walk should call provided callback for each element (read-only visitor)
--FILE--
<?php
$a = array('a'=>1, 'b'=>2, 'c'=>3);
$collect = array();
function _ph7_walk_cb($v,$k){
    global $collect;
    $collect[] = $k . ':' . $v;
}
array_walk($a, '_ph7_walk_cb');
echo implode(',', $collect) . PHP_EOL; // a:1,b:2,c:3
?>
--EXPECT--
a:1,b:2,c:3
--CLEAN--
<?php
unset($a,$collect);
?>
