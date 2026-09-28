--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
stripcslashes reads at most three octal digits and keeps the low byte
--FILE--
<?php
foreach (['\101','\1018','\0','\400','\777','\8','\e'] as $scOctal) {
    printf("%-6s => %s\n", $scOctal, bin2hex(stripcslashes($scOctal)));
}
?>
--EXPECT--
\101   => 41
\1018  => 4138
\0     => 00
\400   => 00
\777   => ff
\8     => 38
\e     => 65
--CLEAN--
<?php
unset($scOctal);
