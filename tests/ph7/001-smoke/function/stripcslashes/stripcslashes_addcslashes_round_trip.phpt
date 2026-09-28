--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
stripcslashes reads back what addcslashes wrote, digits apart
--FILE--
<?php
// Every byte, escaped and read back. The pair is NOT an identity: a digit in
// the mask is escaped as `\0`..`\7`, which stripcslashes reads as OCTAL.
$scRtAll = '';
for ($scRtByte = 0; $scRtByte < 256; $scRtByte++) { $scRtAll .= chr($scRtByte); }
var_dump(stripcslashes(addcslashes($scRtAll, "\0..\377")) === $scRtAll);
$scRtAll = '';
for ($scRtByte = 0; $scRtByte < 48; $scRtByte++) { $scRtAll .= chr($scRtByte); }
for ($scRtByte = 58; $scRtByte < 256; $scRtByte++) { $scRtAll .= chr($scRtByte); }
var_dump(stripcslashes(addcslashes($scRtAll, "\0..\377")) === $scRtAll);
echo bin2hex(addcslashes("\x07\x08\x09\x0a\x0b\x0c\x0d", "\0..\37")), "\n";
?>
--EXPECT--
bool(false)
bool(false)
5c615c625c745c6e5c765c665c72
--CLEAN--
<?php
unset($scRtByte, $scRtAll);
