--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff_assoc should drop entries only when both key and value match;\
values duplicated under other keys must not confuse the result
--FILE--
<?php
// first array has key 'x' with value 1
$a = array('x' => 1);
// second array contains an unrelated key 'z' with same value, and later the
// matching key 'x'.  The bug would keep 'x' because the value lookup returned
// the earlier 'z' node.
$b = array('z' => 1, 'x' => 1);
$r = array_diff_assoc($a, $b);
// expect the entry to be dropped entirely
echo count($r) === 0 ? 'OK' : 'FAIL';
?>
--EXPECT--
OK
--CLEAN--
<?php
unset($a, $b, $r);
