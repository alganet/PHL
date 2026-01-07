--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Valid hex escape sequences in double quoted strings
--FILE--
<?php
// \x41 = 'A'
$s = "\x41";
if ($s === "A") echo "PASS\n";
// \x0A = newline
$s = "\x0A";
if (ord($s) === 10) echo "PASS\n";
// \x7F = DEL
$s = "\x7F";
if (ord($s) === 127) echo "PASS\n";
?>
--EXPECT--
PASS
PASS
PASS