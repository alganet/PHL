--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: fnmatch matches patterns and respects FNM_CASEFOLD
--FILE--
<?php
// Simple match
echo fnmatch('*.txt', 'file.txt') ? "1\n" : "0\n";
// Case insensitive match using FNM_CASEFOLD (if defined), otherwise emulate
$casefold = defined('FNM_CASEFOLD') ? FNM_CASEFOLD : 0x08;
echo fnmatch('*.txt', 'FILE.TXT', $casefold) ? "1\n" : "0\n";
?>
--EXPECT--
1
1
