--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Case folding is ASCII-only: no byte above 0x7F ever changes case
--FILE--
<?php
// php folds A-Z / a-z and nothing else — not Latin-1 high bytes, not UTF-8
// sequences — and does so independently of the C locale. Every byte outside
// the ASCII letter ranges must survive both directions untouched.
$s = "\xE9\xC9 abc-XYZ \xC3\xA9\xC3\x89";
echo bin2hex(strtoupper($s)), "\n";
echo bin2hex(strtolower($s)), "\n";
echo bin2hex(ucfirst("\xE9x")), ' ', bin2hex(lcfirst("\xC9X")), "\n";
echo bin2hex(ucwords("\xE9bc def")), "\n";

// The case-INSENSITIVE comparisons fold the same way: 0xE9 and 0xC9 are the
// Latin-1 case pair, and neither engine treats them as one.
var_dump(strcasecmp("\xE9", "\xC9") !== 0, strcasecmp('ABC', 'abc') === 0);
var_dump(strncasecmp("A\xE9", "a\xC9", 2) !== 0);
var_dump(stripos("x\xC9y", "\xE9"));

$a = ["B\xC9", "b\xE9", 'A'];
natcasesort($a);
echo implode(',', array_map('bin2hex', $a)), "\n";

// ...and so does the NAME resolution that shares the fold.
echo STRTOUPPER('i'), STRTOLOWER('I'), "\n";
?>
--EXPECT--
e9c9204142432d58595a20c3a9c389
e9c9206162632d78797a20c3a9c389
e978 c958
e9626320446566
bool(true)
bool(true)
bool(true)
bool(false)
41,42c9,62e9
Ii
--CLEAN--
<?php
