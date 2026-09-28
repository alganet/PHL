--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
quoted_printable_decode reads a hex pair in either case
--FILE--
<?php
foreach (['=41=42', '=ff', '=FF', '=3D', '=00=FF'] as $qpDecHex) {
    printf("%-8s => %s\n", $qpDecHex, bin2hex(quoted_printable_decode($qpDecHex)));
}
?>
--EXPECT--
=41=42   => 4142
=ff      => ff
=FF      => ff
=3D      => 3d
=00=FF   => 00ff
--CLEAN--
<?php
unset($qpDecHex);
