--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An EMPTY command-line argument keeps its $argv slot (and its place in $argc)
--ARGS--
alpha "" beta ""
--FILE--
<?php
// An empty argument is a real element: dropping it renumbers everything after
// it, so $argv[2] would silently become 'beta' and $argc would read 4.
echo $argc, "\n";
var_export(array_slice($argv, 1));
echo "\n";
echo count($_SERVER['argv']), " ", $_SERVER['argc'], "\n";
?>
--EXPECT--
5
array (
  0 => 'alpha',
  1 => '',
  2 => 'beta',
  3 => '',
)
5 5
--CLEAN--
<?php
