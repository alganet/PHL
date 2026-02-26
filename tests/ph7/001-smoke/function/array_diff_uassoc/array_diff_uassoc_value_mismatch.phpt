--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
When the key comparison callback considers two keys equal but the values differ,
those entries should be kept
--FILE--
<?php
$a = array('a' => 1);
$b = array('A' => 2);
$r = array_diff_uassoc($a, $b, 'strcasecmp');
echo isset($r['a']) && $r['a'] === 1 ? 'OK' : 'FAIL';
?>
--EXPECT--
OK
--CLEAN--
<?php
unset($a, $b, $r);
