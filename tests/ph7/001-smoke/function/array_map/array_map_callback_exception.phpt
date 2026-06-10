--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Exceptions raised by array callback builtins unwind
--DESCRIPTION--
A callback that throws inside array_map / array_filter / array_reduce /
array_walk / usort must propagate the exception out of the builtin instead of
swallowing it and continuing.
--FILE--
<?php
$a = [3, 1, 2];

try { array_map(function ($v) { throw new Exception("map"); }, $a); echo "no\n"; }
catch (Exception $e) { echo $e->getMessage(), "\n"; }

try { array_filter($a, function ($v) { throw new Exception("filter"); }); echo "no\n"; }
catch (Exception $e) { echo $e->getMessage(), "\n"; }

try { array_reduce($a, function ($c, $v) { throw new Exception("reduce"); }); echo "no\n"; }
catch (Exception $e) { echo $e->getMessage(), "\n"; }

try { array_walk($a, function ($v) { throw new Exception("walk"); }); echo "no\n"; }
catch (Exception $e) { echo $e->getMessage(), "\n"; }

try { usort($a, function ($x, $y) { throw new Exception("usort"); }); echo "no\n"; }
catch (Exception $e) { echo $e->getMessage(), "\n"; }

echo "done\n";
?>
--EXPECT--
map
filter
reduce
walk
usort
done
--CLEAN--
<?php
unset($a);
