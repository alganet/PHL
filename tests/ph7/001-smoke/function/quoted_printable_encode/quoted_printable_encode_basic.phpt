--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
quoted_printable_encode escapes control, high and equals bytes
--FILE--
<?php
foreach (['abc', 'a=b', "caf\xc3\xa9", "tab\there", "\x00\x01\x7f", ''] as $qpEncBasic) {
    printf("%-10s => %s\n", bin2hex($qpEncBasic), quoted_printable_encode($qpEncBasic));
}
?>
--EXPECT--
616263     => abc
613d62     => a=3Db
636166c3a9 => caf=C3=A9
7461620968657265 => tab=09here
00017f     => =00=01=7F
           => 
--CLEAN--
<?php
unset($qpEncBasic);
