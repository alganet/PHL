--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
quoted_printable_decode keeps an equals sign that begins nothing
--FILE--
<?php
foreach (['=4', '=zz', '=', 'abc=', '= X', "=\tX", '==41', '=A'] as $qpDecBad) {
    printf("%-8s => %s\n", $qpDecBad, bin2hex(quoted_printable_decode($qpDecBad)));
}
?>
--EXPECT--
=4       => 3d34
=zz      => 3d7a7a
=        => 
abc=     => 616263
= X      => 3d2058
=	X      => 3d0958
==41     => 3d41
=A       => 3d41
--CLEAN--
<?php
unset($qpDecBad);
