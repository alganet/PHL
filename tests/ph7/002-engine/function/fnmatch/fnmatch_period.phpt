--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: fnmatch respects FNM_PERIOD flag
--FILE--
<?php
// Test FNM_PERIOD flag (period matches only itself, not any character)
echo fnmatch('te.t', 'test', FNM_PERIOD) ? "1\n" : "0\n"; // Should match 'te.t' to 'test'
echo fnmatch('te.t', 'te.t', FNM_PERIOD) ? "1\n" : "0\n"; // Should match 'te.t' to 'te.t'
?>
--EXPECT--
0
1