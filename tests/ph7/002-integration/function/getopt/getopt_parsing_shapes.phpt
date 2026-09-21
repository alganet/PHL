--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
getopt() walks $argv: clustering, attached values, repeats, and &$rest_index
--ARGS--
-a -bval -c --long2=xx --long1 --long3=z -a operand --unknown -b y
--FILE--
<?php
// Parsing stops at the first element that is not an option — everything from
// 'operand' on is the caller's to handle, including the -b y that follows it.
// A repeated option collects every occurrence in order; "-bval" attaches its
// value; an optional-value option with nothing attached answers false.
$rest = 'PRESET';
var_export(getopt('ab:c::', ['long1', 'long2:', 'long3::'], $rest));
echo "\nrest=", $rest, " argv[rest]=", $argv[$rest], "\n";
?>
--EXPECT--
array (
  'a' => 
  array (
    0 => false,
    1 => false,
  ),
  'b' => 'val',
  'c' => false,
  'long2' => 'xx',
  'long1' => false,
  'long3' => 'z',
)
rest=8 argv[rest]=operand
--CLEAN--
<?php
