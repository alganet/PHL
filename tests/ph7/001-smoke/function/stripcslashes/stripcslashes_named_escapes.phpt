--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
stripcslashes reads the seven named C escapes
--FILE--
<?php
foreach (['\n','\t','\r','\a','\v','\b','\f','\\\\','\\'] as $scNamed) {
    printf("%-6s => %s\n", $scNamed, bin2hex(stripcslashes($scNamed)));
}
?>
--EXPECT--
\n     => 0a
\t     => 09
\r     => 0d
\a     => 07
\v     => 0b
\b     => 08
\f     => 0c
\\     => 5c
\      => 5c
--CLEAN--
<?php
unset($scNamed);
