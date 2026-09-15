--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Named arguments: parameter errors are catchable Error, execution continues
--FILE--
<?php
function naec($a) { echo "a=$a\n"; }
function naecgen($a) { yield $a; }

try { naec(b: 1); } catch (\Error $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
try { naec(1, a: 2); } catch (\Error $e) { echo "dup: ", $e->getMessage(), "\n"; }
try { $g = naecgen(zz: 1); foreach ($g as $v) { echo $v; } }
catch (\Error $e) { echo "gen: ", $e->getMessage(), "\n"; }

naec(a: 7);
foreach (naecgen(a: 8) as $v) { echo "gen=$v\n"; }
echo "alive\n";
?>
--EXPECT--
Error: Unknown named parameter $b
dup: Named parameter $a overwrites previous argument
gen: Unknown named parameter $zz
a=7
gen=8
alive
