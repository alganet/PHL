--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
preg_replace with zero-width (lookaround/anchor) matches keeps the subject intact
--DESCRIPTION--
Regression: on a zero-width match the replace loop emitted the byte at the SEARCH
start offset to make progress, but a lookbehind/lookahead can place the match
AHEAD of that offset — so the wrong byte was copied and the following character
was corrupted (the camelCase split /(?<=[[:lower:]])(?=[[:upper:]])/ turned
'fooBar' into 'foo far'). It now emits the byte at the match position.
--FILE--
<?php
echo preg_replace('/(?<=[[:lower:]])(?=[[:upper:]])/u', ' ', 'fooBarBaz'), "\n";
echo preg_replace('/(?<=[a-z])(?=[A-Z])/', ' ', 'HTTPRequestParser'), "\n";
echo preg_replace('/(?=.)/', '-', 'abc'), "\n";
echo preg_replace('/(?<=e)(?=T)/u', ' ', 'SystèmeTesté'), "\n";
echo preg_replace('/\b/', '|', 'ab cd'), "\n";
?>
--EXPECT--
foo Bar Baz
HTTPRequest Parser
-a-b-c
Système Testé
|ab| |cd|
--CLEAN--
<?php
