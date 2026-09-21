--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
getopt() consumes a "--" terminator and reports the index after it
--ARGS--
-a -- -b v
--FILE--
<?php
// "--" ends the options and is itself consumed, so $rest_index points at the
// element AFTER it — the -b that follows is an operand, not an option.
$r1 = 'PRESET';
var_export(getopt('ab:', [], $r1));
echo "\nr1=", $r1, "\n";
// A short-option string that declares nothing still reports where it stopped.
$r2 = 'PRESET';
var_export(getopt('', ['long1'], $r2));
echo "\nr2=", $r2, "\n";
?>
--EXPECT--
array (
  'a' => false,
)
r1=3
array (
)
r2=3
--CLEAN--
<?php
