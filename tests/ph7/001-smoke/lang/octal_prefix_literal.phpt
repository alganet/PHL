--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHP 8.1 explicit octal literal 0o / 0O
--FILE--
<?php
echo 0o17000, "\n";
echo 0O777, "\n";
echo 0o1_7000, "\n";
echo 0o0, "\n";
// Coexists with the legacy and other bases.
echo 017, " ", 0x1F, " ", 0b101, "\n";
// Used in a bitmask, like real code (fstat mode bits).
$mode = 0o0100644;
echo ($mode & 0o170000) === 0o100000 ? "regfile\n" : "other\n";
var_dump(0o755);
?>
--EXPECT--
7680
511
7680
0
15 31 5
regfile
int(493)
--CLEAN--
<?php
