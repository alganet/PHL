--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
getopt() skips an unknown option and drops one whose required value is missing
--ARGS--
-z -ab
--FILE--
<?php
// -z is not declared: skipped, parsing continues. In "-ab" the cluster gives
// -a, then -b needs a value and there is nothing left — php drops the option
// rather than inventing false for it.
$r1 = 'PRESET';
var_export(getopt('ab:', [], $r1));
echo "\nr1=", $r1, "\n";
// The undefined-variable form of the out-param is created by the call.
getopt('a', [], $getopt_undef_rest);
var_export($getopt_undef_rest);
echo "\n";
?>
--EXPECT--
array (
  'a' => false,
)
r1=3
3
--CLEAN--
<?php
