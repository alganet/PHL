--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_flip basic behaviour with duplicate values
--FILE--
<?php
$a = array('a' => 'v1', 'b' => 'v2', 'c' => 'v1');
$flip = array_flip($a);
// the earlier and later v1 should leave the last key,
// and v2 should map to 'b'.
$ok = ($flip['v1'] === 'c' && $flip['v2'] === 'b');
echo $ok ? "PASS" : "FAIL";
?>
--EXPECT--
PASS
--CLEAN--
<?php
unset($a, $flip, $ok);
