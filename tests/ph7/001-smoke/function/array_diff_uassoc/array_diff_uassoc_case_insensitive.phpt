--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_diff_uassoc must apply callback to keys, allowing case-insensitive matching
--FILE--
<?php
$a = array('A' => 1);
$b = array('a' => 1);
// callback compares keys without regard to case
$r = array_diff_uassoc($a, $b, 'strcasecmp');
echo count($r) === 0 ? 'OK' : 'FAIL';
?>
--EXPECT--
OK
--CLEAN--
<?php
unset($a, $b, $r);
