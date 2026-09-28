--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
stripcslashes reads one or two hex digits behind \x
--FILE--
<?php
foreach (['\x41','\x4','\x4g','\xg','\x','\xff','\X41'] as $scHex) {
    printf("%-6s => %s\n", $scHex, bin2hex(stripcslashes($scHex)));
}
?>
--EXPECT--
\x41   => 41
\x4    => 04
\x4g   => 0467
\xg    => 7867
\x     => 78
\xff   => ff
\X41   => 583431
--CLEAN--
<?php
unset($scHex);
