--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
$GLOBALS["GLOBALS"] and dynamic-name writes do not clobber the live view
--FILE--
<?php
// GLOBALS self-key creates a separate symtable entry, view intact
$GLOBALS["GLOBALS"] = 5;
echo $GLOBALS["GLOBALS"] ?? "none", "|", is_array($GLOBALS) ? "arr" : "corrupt", "\n";
$q = 7; echo $GLOBALS["q"], "\n";
// dynamic-name write behaves the same (php: no compile fatal)
$n = "GLOBALSX"; $n = "GLOBALS";
${$n} = 6;
echo $GLOBALS["GLOBALS"] ?? "none", "|", is_array($GLOBALS) ? "arr" : "corrupt", "\n";
--EXPECT--
5|arr
7
6|arr
--CLEAN--
<?php
unset($GLOBALS["GLOBALS"], $q, $n);
