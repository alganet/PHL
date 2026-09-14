--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sort/rsort/usort reindex keys even for a single-element array
--FILE--
<?php
$a = ['foo' => 'bar']; sort($a);  var_export($a); echo "\n";
$b = ['foo' => 'bar']; rsort($b); var_export($b); echo "\n";
$c = ['x' => 3];       usort($c, fn($p, $q) => $p <=> $q); var_export($c); echo "\n";
$d = [];               sort($d);  var_export($d); echo "\n";
// asort keeps the association (contrast)
$e = ['z' => 1];       asort($e); var_export($e); echo "\n";
?>
--EXPECT--
array (
  0 => 'bar',
)
array (
  0 => 'bar',
)
array (
  0 => 3,
)
array (
)
array (
  'z' => 1,
)
--CLEAN--
<?php
