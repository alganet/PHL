--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_match sets an undefined $matches to an empty array on no match
--FILE--
<?php
// On no match, an undefined $m must become an empty array (not stay null).
$r = preg_match('/(\d+)/', 'abc', $m);
echo $r . "\n";
echo (is_array($m) ? "array" : gettype($m)) . "\n";
echo count($m) . "\n";
?>
--EXPECT--
0
array
0
--CLEAN--
<?php
