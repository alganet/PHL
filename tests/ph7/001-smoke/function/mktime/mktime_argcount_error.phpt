--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mktime()/gmmktime() throw ArgumentCountError beyond 6 arguments (PHP 8)
--FILE--
<?php
// PHP 8 dropped the legacy $is_dst 7th parameter, so mktime()/gmmktime() accept
// at most 6 arguments and throw a catchable ArgumentCountError otherwise.
try { mktime(1, 2, 3, 4, 5, 6, 7); }
catch (\ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
try { mktime(1, 2, 3, 4, 5, 6, 7, 8); }
catch (\ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
try { gmmktime(1, 2, 3, 4, 5, 6, 7); }
catch (\ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
// the 6-argument form is still valid (value is timezone-dependent, so just
// assert it returns an int)
echo is_int(mktime(1, 2, 3, 4, 5, 6)) ? "mktime6-ok\n" : "mktime6-bad\n";
echo is_int(gmmktime(1, 2, 3, 4, 5, 6)) ? "gmmktime6-ok\n" : "gmmktime6-bad\n";
?>
--EXPECT--
mktime() expects at most 6 arguments, 7 given
mktime() expects at most 6 arguments, 8 given
gmmktime() expects at most 6 arguments, 7 given
mktime6-ok
gmmktime6-ok
--CLEAN--
<?php
